module Api
  class DeviceContentController < ApplicationController
    include UserAuthenticatable

    # Hardware endpoints retain the existing unauthenticated device API contract.
    # Character GET is also open so the device can fetch it, but a presented login is still checked.
    before_action :authenticate_user!, except: [ :heartbeat, :audio, :create_event, :create_voice, :character ]
    before_action :authenticate_character_reader!, only: :character
    before_action :load_device

    def heartbeat
      @device.update!(last_seen_at: Time.current)
      WebPushNotice.recover_pending!(@device)
      payload = @setting.with_lock do
        settings_json.merge(
          device_id: @device.id,
          leave_audio: DeviceAudioStore.metadata(@device, @setting, "leave"),
          arrive_audio: DeviceAudioStore.metadata(@device, @setting, "arrive"),
          character: {
            version: @setting.character_version,
            path: "/api/devices/#{@device.id}/character"
          }
        )
      end
      render json: payload
    end

    def audio
      return render_error(:not_found, "not_found", "Unknown audio slot") unless DeviceAudioStore::SLOTS.include?(params[:slot])

      bytes = DeviceAudioStore.bytes(@device, params[:slot])
      response.headers["Cache-Control"] = "no-store"
      response.headers["Content-Length"] = bytes.bytesize.to_s
      send_data bytes, type: "audio/wav", disposition: "inline", filename: "#{params[:slot]}.wav"
    end

    def status
      registered = @device.device_audios.index_by(&:slot)
      arrive = registered["arrive"]
      leave = registered["leave"]
      arrive_audio = arrive ? DeviceAudioStore.metadata(@device, @setting, "arrive") : nil
      render json: {
        device_id: @device.id, role: @device.role_for(current_user), online: @device.online?, last_seen_at: @device.last_seen_at&.iso8601,
        message_count: arrive ? 1 : 0,
        message_audio: arrive_audio,
        leave_audio: leave ? DeviceAudioStore.metadata(@device, @setting, "leave") : nil,
        arrive_audio: arrive_audio,
        message_updated_at: @setting.message_uploaded_at&.iso8601
      }
    end

    def settings
      render json: settings_json
    end

    def update_settings
      unless @device.resident?(current_user)
        return render_error(:forbidden, "resident_only", "設定を変更できるのは、この家の居住者だけです。")
      end

      if @setting.update(params.permit(:volume, :leave_greeting_enabled, :arrive_greeting_enabled))
        render json: settings_json
      else
        render_error(:unprocessable_entity, "validation_error", @setting.errors.full_messages.join(", "))
      end
    end

    def message
      slot = params[:slot].presence || "arrive" # Current recorder is a welcome message.
      return render_error(:unprocessable_entity, "validation_error", "Invalid slot") unless DeviceAudioStore::SLOTS.include?(slot)

      upload = params[:file]
      bytes = upload.read(WavAudio::MAX_BYTES + 1) if upload.respond_to?(:read)
      unless WavAudio.valid?(bytes)
        return render_error(:unprocessable_entity, "validation_error", "WAV must be PCM16 mono 16kHz, at most 5 seconds / 200KiB")
      end
      sender = params[:sender_name].to_s.strip.first(100)
      DeviceAudioStore.write!(@device, @setting, slot, bytes, sender, user: current_user)
      render json: DeviceAudioStore.metadata(@device, @setting, slot), status: :created
    end

    def character
      return render_error(:not_found, "not_found", "顔はまだ保存されていません。") if @setting.character.blank?

      render json: @setting.character
    end

    def extract_character
      return resident_only unless @device.resident?(current_user)

      upload = params[:file]
      unless upload.respond_to?(:read)
        return render_error(:unprocessable_entity, "validation_error", "写真は JPEG、PNG、WebP で、4MB 以下にしてください。")
      end

      bytes = upload.read(CharacterExtractor::MAX_BYTES + 1)
      render json: CharacterExtractor.call(bytes, image_mime(upload))
    rescue CharacterExtractor::InvalidImage
      render_error(:unprocessable_entity, "validation_error", "写真は JPEG、PNG、WebP で、4MB 以下にしてください。")
    rescue CharacterExtractor::Error => e
      Rails.logger.warn("character extract failed: #{e.message}")
      render_error(:service_unavailable, "character_unavailable", "写真から顔を作れませんでした。時間をおいてもう一度試してください。")
    end

    def update_character
      return resident_only unless @device.resident?(current_user)

      character = CharacterCatalog.accept(character_params.to_h)
      return render_error(:unprocessable_entity, "validation_error", "顔のパーツに、一覧にない値が含まれています。") unless character

      @setting.with_lock do
        @setting.update!(character: character, character_version: @setting.character_version + 1)
      end
      render json: @setting.character
    end

    def events
      limit = params.fetch(:limit, 50).to_i.clamp(1, 100)
      events = @device.home_events.order(occurred_at: :desc).limit(limit).to_a
      voice_ids = HomeEventVoice.where(home_event_id: events.map(&:id)).pluck(:home_event_id)
      render json: events.map { |e| event_json(e, voice_available: voice_ids.include?(e.id)) }
    end

    def voice
      event = @device.home_events.find_by(id: params[:event_id])
      voice = event&.home_event_voice
      return render_error(:not_found, "not_found", "録音が見つかりません。") unless voice

      response.headers["Cache-Control"] = "private, no-store"
      send_data voice.wav, type: "audio/wav", disposition: "inline", filename: "voice.wav"
    end

    def show_event
      event = @device.home_events.find_by(id: params[:event_id])
      return render_error(:not_found, "not_found", "できごとが見つかりません。") unless event
      render json: event_json(event, voice_available: event.home_event_voice.present?)
    end

    def create_event
      id = params[:event_id]
      type = params[:event_type]
      time = params[:occurred_at]
      unless id.is_a?(String) && id.present? && id.bytesize <= 255 &&
             HomeEvent.event_types.key?(type) && time.is_a?(String) && time.match?(/(?:Z|\+00:00)\z/)
        return render_error(:unprocessable_entity, "validation_error", "Invalid event fields")
      end
      occurred_at = Time.iso8601(time)
      event = HomeEvent.find_by(id: id)
      if event && event.device_id != @device.id
        return render_error(:unprocessable_entity, "validation_error", "Event ID belongs to another device")
      end
      created = false
      @device.with_lock do
        event = @device.home_events.find_by(id: id)
        unless event
          event = @device.home_events.create!(id: id, event_type: type, occurred_at: occurred_at)
          created = true
        end
        @device.update!(last_seen_at: Time.current)
      end
      notify_messengers(event) if params[:voice].to_s == "skipped"
      render json: event_json(event), status: created ? :created : :ok
    rescue ArgumentError, ActiveRecord::RecordInvalid, ActiveRecord::RecordNotUnique
      render_error(:unprocessable_entity, "validation_error", "Invalid event")
    end

    def create_voice
      event = @device.home_events.find_by(id: params[:event_id])
      return render_error(:not_found, "not_found", "Unknown event") unless event

      bytes = voice_bytes
      unless WavAudio.valid_reply?(bytes)
        return render_error(:unprocessable_entity, "validation_error", "WAV must be PCM16 mono 16kHz, at most 10 seconds / 320KiB")
      end

      created = false
      event.with_lock do
        voice = event.home_event_voice || event.build_home_event_voice
        created = voice.new_record?
        voice.update!(wav: bytes)
      end
      notify_messengers(event, wav: bytes)
      render json: event_json(event), status: created ? :created : :ok
    end

    private

    def authenticate_character_reader!
      return if request.headers["Authorization"].blank?

      @current_user = user_from_token
      render_unauthorized unless current_user
    end

    def resident_only
      render_error(:forbidden, "resident_only", "顔を変更できるのは、この家の居住者だけです。")
    end

    def image_mime(upload)
      mime = upload.content_type.to_s.split(";").first.to_s.strip.downcase
      return mime if CharacterExtractor::MIME_TYPES.key?(mime)

      {
        ".jpg" => "image/jpeg",
        ".jpeg" => "image/jpeg",
        ".png" => "image/png",
        ".webp" => "image/webp"
      }[File.extname(upload.original_filename.to_s).downcase]
    end

    def character_params
      params.permit(:bg, :hairColor, :hair, :eyeColor, :eyes, :glasses, :beard, :mouth, extras: [], uncertain: [])
    end

    def load_device
      @device = Device.find_by(id: params[:device_id])
      return render_error(:not_found, "not_found", "Unknown device") unless @device
      if current_user && !@device.accessible_to?(current_user)
        return render_error(:forbidden, "device_forbidden", "このデバイスを操作する権限がありません。デバイス管理で登録したデバイスを選択してください。")
      end

      @setting = DeviceSetting.find_or_create_by!(device: @device)
    end

    def settings_json
      @setting.slice(:leave_greeting_enabled, :arrive_greeting_enabled, :volume)
    end

    def event_json(event, voice_available: nil)
      payload = { event_id: event.id, event_type: event.event_type, occurred_at: event.occurred_at.iso8601 }
      payload[:voice_available] = voice_available unless voice_available.nil?
      payload
    end

    def voice_bytes
      upload = params[:file]
      if upload.respond_to?(:read)
        upload.read(WavAudio::REPLY_MAX_BYTES + 1)
      elsif request.media_type == "audio/wav"
        request.body.read(WavAudio::REPLY_MAX_BYTES + 1)
      end
    end

    # The first claim creates push outbox rows. A later upload still replaces the WAV.
    def notify_messengers(event, wav: nil)
      return if wav.nil? && event.reload.home_event_voice.present?

      claimed = HomeEvent.where(id: event.id, messengers_notified_at: nil).update_all(
        voice_skipped: wav.nil?,
        messengers_notified_at: Time.current,
        updated_at: Time.current
      )
      return unless claimed == 1

      HomeEvent.where(id: event.id).update_all(messengers_notified_at: nil, updated_at: Time.current) unless HomeEventNotice.deliver!(event, wav: wav)
    end

    def render_error(status, code, message)
      render json: { error: { code: code, message: message } }, status: status
    end
  end
end
