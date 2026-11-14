class PushWorkerController < ApplicationController
  def show
    script = Rails.root.join("spa", "push-worker.js")
    return head :not_found unless script.file?

    response.headers["Cache-Control"] = "no-store"
    response.headers["Service-Worker-Allowed"] = "/"
    send_file script, type: "application/javascript", disposition: "inline"
  end
end
