# Development/build image for the viduce-broadcast web app.
# Isolated from the repo's Rust/C++ engine image — this is a Node/TypeScript app.
FROM node:20-bookworm-slim AS base

WORKDIR /app

# Install dependencies first so this layer is cached unless the lockfile changes.
COPY package.json package-lock.json ./
RUN npm ci

# Bring in the rest of the source (shadowed by a bind mount during dev).
COPY . .

# Vite dev server.
EXPOSE 5173

# Bind to 0.0.0.0 so the port is reachable from the host (the host browser
# still connects via http://localhost:5173, which is a secure context).
CMD ["npm", "run", "dev", "--", "--host", "0.0.0.0"]
