Rails.application.routes.draw do
  # Define your application routes per the DSL in https://guides.rubyonrails.org/routing.html

  # Reveal health status on /up that returns 200 if the app boots with no exceptions, otherwise 500.
  # Can be used by load balancers and uptime monitors to verify that the app is live.
  get "up" => "rails/health#show", as: :rails_health_check

  namespace :api do
    get "health", to: "health#show"
    post "sessions", to: "sessions#create"
    resources :users, only: [ :create ]
    resources :user_sessions, only: [ :create ]
  end

  # Vite のビルド成果物に存在しない画面パスは index.html に戻す。
  root "spa#index"
  get "*path", to: "spa#index", constraints: ->(request) {
    !request.path.start_with?("/api") && request.format.html?
  }
end
