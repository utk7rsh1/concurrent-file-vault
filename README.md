# P2P File Vault with Version Control
## EGC 301P - Operating Systems Lab Mini Project

A multi-user version-controlled file storage vault demonstrating modern operating systems concepts including socket communication, inter-process communication (IPC), advisory locking, multithreading, and semaphore-based synchronization.

---

## Features & Implementation details

- **Role-Based Authorization**: Owner, Contributor, and Viewer privileges mapped across 5 user accounts. Owner has exclusive access to deletes and logs.
- **Concurrent Access & Advisory Locking**: Protects files using both `pthread_rwlock_t` (thread level) and `fcntl` advisory locks (process level).
- **Multi-Process Communication (IPC)**:
  - **Shared Memory & POSIX Semaphore**: live version manifest updated by the indexer daemon and read by the server.
  - **Message Queue**: notifications dispatched by the server thread on successful pushes to be processed by the indexer daemon.
  - **Named Pipes (FIFOs)**: conflict logger reporting version divergence.
  - **Unnamed Pipes**: unified `diff` child execution redirection.

---

## Build

Compile the server, client, and indexer binaries:
```bash
make
```

## Running the System

Open three terminal windows:

1. **Terminal 1 – Central Server**
   ```bash
   ./vault_server
   ```

2. **Terminal 2 – Indexer Daemon**
   ```bash
   ./vault_indexer
   ```

3. **Terminal 3 – Client Shell**
   ```bash
   ./vault_client 127.0.0.1 8080
   ```

---

## Example Session

Authenticate yourself:
```bash
vault> auth alice alice123
```

Push a file to the server (automatically creates version v1):
```bash
vault> push my_report.txt ./report.txt
```

List the current status:
```bash
vault> list
```

Pull a specific version of a file:
```bash
vault> pull my_report.txt 1 downloaded_report.txt
```

Check the system manifest status:
```bash
vault> status
```

Quit client shell:
```bash
vault> quit
```
