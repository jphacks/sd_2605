class SpaController < ApplicationController
  INDEX_HTML = Rails.root.join("spa", "index.html")

  def index
    return head :not_found unless INDEX_HTML.file?

    send_file INDEX_HTML, type: "text/html", disposition: "inline"
  end
end
