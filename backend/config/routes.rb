Rails.application.routes.draw do
  # Define your application routes per the DSL in https://guides.rubyonrails.org/routing.html

  # Reveal health status on /up that returns 200 if the app boots with no exceptions, otherwise 500.
  # Can be used by load balancers and uptime monitors to verify that the app is live.
  get "up" => "rails/health#show", as: :rails_health_check

  namespace :api do
    get "health", to: "health#show"
    post "sessions", to: "sessions#create"
    resources :devices, only: [ :create ]
    scope "devices/:device_id", controller: :device_content do
      post "heartbeat", action: :heartbeat
      get "audio/:slot", action: :audio
      post "events", action: :create_event
      get "events", action: :events
      get "status", action: :status
      get "settings", action: :settings
      patch "settings", action: :update_settings
      post "message", action: :message
    end
    get "auth_config", to: "auth_config#show"
    resources :users, only: [ :create ]
    resources :user_sessions, only: [ :create ]
    resources :google_sessions, only: [ :create ]
  end

  # React Router の画面パスはすべて index.html を返す。/rails は Active Storage などの組み込みルート。
  root "spa#index"
  get "*path", to: "spa#index", constraints: ->(request) {
    !request.path.start_with?("/api", "/rails") && request.format.html?
  }
end
