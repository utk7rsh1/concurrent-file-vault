CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -I.
LDFLAGS = -lpthread -lrt

SERVER_SRCS = server/main.c server/auth.c server/vault.c server/ipc.c
INDEXER_SRCS = indexer/indexer.c
CLIENT_SRCS = client/client.c

all: vault_server vault_indexer vault_client

vault_server: $(SERVER_SRCS) common/protocol.h server/auth.h server/vault.h server/ipc.h
	$(CC) $(CFLAGS) -o vault_server $(SERVER_SRCS) $(LDFLAGS)

vault_indexer: $(INDEXER_SRCS) common/protocol.h
	$(CC) $(CFLAGS) -o vault_indexer $(INDEXER_SRCS) $(LDFLAGS)

vault_client: $(CLIENT_SRCS) common/protocol.h
	$(CC) $(CFLAGS) -o vault_client $(CLIENT_SRCS) $(LDFLAGS)

clean:
	rm -f vault_server vault_indexer vault_client
	rm -rf vault_storage vault_index.log conflict.log /tmp/vault_conflict_pipe

.PHONY: all clean
