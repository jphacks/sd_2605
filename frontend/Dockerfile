FROM node:22

WORKDIR /app

COPY package.json ./
RUN npm install

COPY . .

EXPOSE 5173

CMD ["sh", "-c", "npm install && exec npm run dev -- --host 0.0.0.0 --port 5173"]
