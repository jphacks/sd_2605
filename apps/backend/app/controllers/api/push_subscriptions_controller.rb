module Api
  class PushSubscriptionsController < ApplicationController
    include UserAuthenticatable
    before_action :authenticate_user!

    def show_config
      render json: {
        configured: WebPushSettings.configured?,
        public_key: WebPushSettings.configured? ? WebPushSettings.public_key : nil,
        eligible: eligible?
      }
    end

    def check
      return invalid_subscription unless valid_endpoint_param?
      render json: { subscribed: current_subscription.present? }
    end

    def create
      return forbidden unless eligible?
      return unavailable unless WebPushSettings.configured?
      data = params.require(:subscription).permit(:endpoint, keys: [ :p256dh, :auth ])
      return invalid_subscription unless data[:endpoint].is_a?(String) && data[:endpoint].bytesize <= 4096

      subscription = PushSubscription.find_or_initialize_by(endpoint_digest: PushSubscription.digest(data[:endpoint]))
      if subscription.persisted? && subscription.user_id != current_user.id &&
          (!ActiveSupport::SecurityUtils.secure_compare(subscription.p256dh, data.dig(:keys, :p256dh).to_s) ||
           !ActiveSupport::SecurityUtils.secure_compare(subscription.auth, data.dig(:keys, :auth).to_s))
        return invalid_subscription
      end
      # Explicit opt-in transfers this browser endpoint when its user switches accounts.
      subscription.assign_attributes(user: current_user, endpoint: data[:endpoint], p256dh: data.dig(:keys, :p256dh), auth: data.dig(:keys, :auth))
      return invalid_subscription unless subscription.save
      render json: { subscribed: true }, status: :created
    rescue ActionController::ParameterMissing, ActiveRecord::RecordNotUnique
      invalid_subscription
    end

    def destroy
      return invalid_subscription unless valid_endpoint_param?
      current_subscription&.destroy!
      head :no_content
    end

    def test
      return forbidden unless eligible?
      return unavailable unless WebPushSettings.configured?
      return invalid_subscription unless valid_endpoint_param?
      subscription = current_subscription
      return render_error(:not_found, "not_subscribed", "先にこのブラウザーの通知を有効にしてください。") unless subscription

      result = WebPushTransport.deliver(subscription, {
        title: "おかえりんく", body: "ブラウザー通知のテストです。", tag: "notification-test", url: "/history"
      })
      return render_error(:gone, "subscription_expired", "通知の登録が切れています。もう一度有効にしてください。") if result == :expired
      return render_error(:service_unavailable, "push_failed", "通知を送信できませんでした。時間をおいて試してください。") unless result == :sent
      render json: { accepted: true }
    end

    private

    def eligible?
      current_user.device_memberships.where(role: "messenger").exists?
    end

    def valid_endpoint_param?
      params[:endpoint].is_a?(String) && params[:endpoint].bytesize.between?(1, 4096)
    end

    def current_subscription
      current_user.push_subscriptions.find_by(endpoint_digest: PushSubscription.digest(params[:endpoint]))
    end

    def invalid_subscription
      render_error(:unprocessable_entity, "invalid_subscription", "通知の登録情報を確認できませんでした。")
    end

    def unavailable
      render_error(:service_unavailable, "push_unavailable", "通知機能は準備中です。時間をおいて試してください。")
    end

    def forbidden
      render_error(:forbidden, "messenger_only", "通知を受け取れるのは、家の差し出し手として参加しているアカウントです。")
    end

    def render_error(status, code, message)
      render json: { error: { code: code, message: message } }, status: status
    end
  end
end
