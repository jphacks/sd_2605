FROM ruby:3.3.11

RUN apt-get update -qq \
  && apt-get install -y --no-install-recommends build-essential libpq-dev \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Gemfile ./
RUN bundle install

COPY . .

EXPOSE 3000

CMD ["bash", "-c", "bundle install && rm -f tmp/pids/server.pid && bundle exec rails db:prepare && exec bundle exec rails server -b 0.0.0.0 -p 3000"]
