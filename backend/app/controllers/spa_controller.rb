class SpaController < ApplicationController
  def index
    html = Rails.public_path.join("index.html")
    return head :not_found unless html.file?

    send_file html, type: "text/html", disposition: "inline"
  end
end
