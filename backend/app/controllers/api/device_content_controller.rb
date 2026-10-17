module Api
  class DeviceContentController < ApplicationController
    include UserAuthenticatable

    # Hardware endpoints retain the existing unauthenticated device API contract.
    before_action :authenticate_user!, except: [ :heartbeat, :audio, :create_event ]
    before_action :load_device

    def heartbeat
      @device.update!(last_seen_at: Time.current)
      payload = @setting.with_lock do
        settings_json.merge(
          device_id: @device.id,
          leave_audio: DeviceAudioStore.metadata(@device, @setting, "leave"),
          arrive_audio: DeviceAudioStore.metadata(@device, @setting, "arrive")
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
      audio = @device.device_audios.find_by(slot: "arrive")
      render json: {
        device_id: @device.id, online: @device.online?, last_seen_at: @device.last_seen_at&.iso8601,
        message_count: audio ? 1 : 0,
        message_audio: audio ? DeviceAudioStore.metadata(@device, @setting, "arrive") : nil,
        message_updated_at: @setting.message_uploaded_at&.iso8601
      }
    end

    def settings
      render json: settings_json
    end

    def update_settings
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
      DeviceAudioStore.write!(@device, @setting, slot, bytes, sender)
      render json: DeviceAudioStore.metadata(@device, @setting, slot), status: :created
    end

    def events
      limit = params.fetch(:limit, 50).to_i.clamp(1, 100)
      render json: @device.home_events.order(occurred_at: :desc).limit(limit).map { |e| event_json(e) }
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
      render json: event_json(event), status: created ? :created : :ok
    rescue ArgumentError, ActiveRecord::RecordInvalid, ActiveRecord::RecordNotUnique
      render_error(:unprocessable_entity, "validation_error", "Invalid event")
    end

    private

    def load_device
      @device = Device.find_by(id: params[:device_id])
      return render_error(:not_found, "not_found", "Unknown device") unless @device
      return render_unauthorized if current_user && @device.user_id != current_user.id

      @setting = DeviceSetting.find_or_create_by!(device: @device)
    end

    def settings_json
      @setting.slice(:leave_greeting_enabled, :arrive_greeting_enabled, :volume)
    end

    def event_json(event)
      { event_id: event.id, event_type: event.event_type, occurred_at: event.occurred_at.iso8601 }
    end

    def render_error(status, code, message)
      render json: { error: { code: code, message: message } }, status: status
    end
  end
end
