# Arquivo: docker-compose.yml
# Subir:   docker compose up -d
# Parar:   docker compose down
# Logs:    docker compose logs -f

docker compose -f ./src/rabbitmq/docker-compose.yml up -d

# ─────────────────────────────────────────────────────────────
# Comandos úteis
# ─────────────────────────────────────────────────────────────
# Listar filas:
docker exec rabbitmq rabbitmqctl list_queues name messages consumers

# Listar exchanges:
docker exec rabbitmq rabbitmqctl list_exchanges

# Listar bindings:
docker exec rabbitmq rabbitmqctl list_bindings

# Ver conexões ativas:
docker exec rabbitmq rabbitmqctl list_connections

# Purgar fila (apagar mensagens pendentes):
docker exec rabbitmq rabbitmqctl purge_queue task_queue

# Habilitar plugin de tracing (útil para debug):
docker exec rabbitmq rabbitmq-plugins enable rabbitmq_tracing
