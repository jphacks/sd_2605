class PushDeliveryJob < ApplicationJob
  queue_as :default

  def perform(delivery_id)
    delivery = PushDelivery.find_by(id: delivery_id)
    return unless delivery
    retry_at = nil

    delivery.with_lock do
      next if delivery.sent_at || delivery.canceled_at || delivery.attempts >= PushDelivery::MAX_ATTEMPTS
      next if delivery.next_attempt_at && delivery.next_attempt_at > Time.current

      subscription = delivery.push_subscription
      event = delivery.home_event
      unless event.device.messenger?(subscription.user)
        delivery.update!(canceled_at: Time.current)
        next
      end

      delivery.update!(attempts: delivery.attempts + 1)
      result = WebPushTransport.deliver(subscription, WebPushNotice.payload(event))
      case result
      when :sent
        # Provider acknowledgement, not proof of display on the user's screen.
        delivery.update!(sent_at: Time.current, next_attempt_at: nil)
      when :failed
        if delivery.attempts < PushDelivery::MAX_ATTEMPTS
          retry_at = Time.current + [ 15 * (2**delivery.attempts), 300 ].min.seconds
          delivery.update!(next_attempt_at: retry_at)
        end
      end
    end
    self.class.set(wait_until: retry_at).perform_later(delivery_id) if retry_at
  rescue ActiveRecord::RecordNotFound
    # Unsubscribing/deleting a membership may remove the outbox while a job is queued.
  end
end
