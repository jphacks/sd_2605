class JsonWebToken
  ALGORITHM = "HS256"
  EXPIRY = 7.days

  def self.encode(user_id)
    expires_at = EXPIRY.from_now
    token = JWT.encode({ sub: user_id, exp: expires_at.to_i }, secret, ALGORITHM)
    [ token, expires_at ]
  end

  def self.decode(token)
    JWT.decode(token, secret, true, { algorithm: ALGORITHM }).first
  end

  def self.secret
    ENV["JWT_SECRET"].presence || Rails.application.secret_key_base
  end
  private_class_method :secret
end
