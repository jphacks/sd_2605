require "base64"
require "digest"
require "uri"

class PushSubscription < ApplicationRecord
  belongs_to :user
  has_many :push_deliveries, dependent: :destroy

  before_validation { self.endpoint_digest = self.class.digest(endpoint) }
  validates :endpoint, presence: true, length: { maximum: 4096 }
  validates :endpoint_digest, presence: true, uniqueness: true
  validates :p256dh, :auth, presence: true
  validate :valid_push_endpoint
  validate :valid_encryption_keys

  def self.digest(endpoint)
    Digest::SHA256.hexdigest(endpoint.to_s)
  end

  private

  def valid_push_endpoint
    uri = URI.parse(endpoint.to_s)
    host = uri.host.to_s.downcase
    allowed = %w[fcm.googleapis.com push.services.mozilla.com web.push.apple.com notify.windows.com].include?(host) ||
      %w[.push.services.mozilla.com .push.apple.com .notify.windows.com].any? { |suffix| host.end_with?(suffix) }
    errors.add(:endpoint, "is not a supported push service") unless uri.is_a?(URI::HTTPS) && uri.port == 443 &&
      uri.userinfo.nil? && uri.fragment.nil? && allowed
  rescue URI::InvalidURIError
    errors.add(:endpoint, "is invalid")
  end

  def valid_encryption_keys
    public_bytes = decode_key(p256dh, 65)
    auth_bytes = decode_key(auth, 16)
    valid_point = false
    if public_bytes && public_bytes.getbyte(0) == 4
      point = OpenSSL::PKey::EC::Point.new(OpenSSL::PKey::EC::Group.new("prime256v1"), OpenSSL::BN.new(public_bytes, 2))
      valid_point = point.on_curve?
    end
    errors.add(:p256dh, "is invalid") unless valid_point
    errors.add(:auth, "is invalid") unless auth_bytes
  rescue OpenSSL::PKey::EC::Point::Error, OpenSSL::BNError
    errors.add(:p256dh, "is invalid")
  end

  def decode_key(value, size)
    return unless value.is_a?(String) && value.bytesize <= 100 && value.match?(/\A[A-Za-z0-9_-]+={0,2}\z/)
    bytes = Base64.urlsafe_decode64(value)
    bytes if bytes.bytesize == size
  rescue ArgumentError
    nil
  end
end
