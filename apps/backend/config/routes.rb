Rails.application.routes.draw do
  # Define your application routes per the DSL in https://guides.rubyonrails.org/routing.html

  # Reveal health status on /up that returns 200 if the app boots with no exceptions, otherwise 500.
  # Can be used by load balancers and uptime monitors to verify that the app is live.
  get "up" => "rails/health#show", as: :rails_health_check
  get "push-worker.js", to: "push_worker#show"

  namespace :api do
    get "health", to: "health#show"
    post "sessions", to: "sessions#create"
    resources :devices, only: [ :index, :create ] do
      resources :invitations, only: [ :create ], controller: "device_invitations"
    end
    post "invitations/accept", to: "invitations#accept"
    scope "devices/:device_id", controller: :device_content do
      post "heartbeat", action: :heartbeat
      get "audio/:slot", action: :audio
      post "events", action: :create_event
      post "events/:event_id/voice", action: :create_voice
      get "events/:event_id/voice", action: :voice
      get "events", action: :events
      get "events/:event_id", action: :show_event
      get "status", action: :status
      get "settings", action: :settings
      patch "settings", action: :update_settings
      post "message", action: :message
      get "character", action: :character
      post "character/extract", action: :extract_character
      put "character", action: :update_character
    end
    get "auth_config", to: "auth_config#show"
    get "push_config", to: "push_subscriptions#show_config"
    post "push_subscriptions/check", to: "push_subscriptions#check"
    post "push_subscriptions/test", to: "push_subscriptions#test"
    post "push_subscriptions", to: "push_subscriptions#create"
    delete "push_subscriptions", to: "push_subscriptions#destroy"
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
