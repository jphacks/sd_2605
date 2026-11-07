class HomeEventMailer < ApplicationMailer
  def messenger_notice
    event = params.fetch(:event)
    @label = event.left_home? ? "お出かけしました" : "帰宅しました"
    wav = params[:wav]
    @voice_attached = wav.present?
    attachments["voice.wav"] = { mime_type: "audio/wav", content: wav } if @voice_attached
    mail(to: params.fetch(:user).email, subject: @label) do |format|
      format.text
    end
  end
end
