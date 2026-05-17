docker-compose -f ../docker-compose.yml down --remove-orphans
docker-compose -f ../docker-compose.debug.yml down --remove-orphans
docker image prune -f