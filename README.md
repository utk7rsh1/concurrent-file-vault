# P2P File Vault with Version Control
## EGC 301P - Operating Systems Lab Mini Project

A multi-user version-controlled file storage vault demonstrating modern operating systems concepts including socket communication, inter-process communication (IPC), advisory locking, multithreading, and semaphore-based synchronization.

---

## Architecture & Design Choices

During the development of this vault, several key OS primitives were selected to solve specific concurrency and communication problems:

### 1. Two-Layer Locking Strategy (Process vs. Thread Concurrency)
- **Problem**: I needed to allow multiple readers to pull files concurrently while ensuring that file pushes (writes) are exclusive. However, standard thread locks (`pthread_rwlock_t`) only protect within the server process, while process locks (`fcntl`) only protect across independent processes.
- **Solution**: I implemented a two-layer lock:
  1. **Thread-Level**: A global `pthread_rwlock_t` serializes client-handler threads inside the `vault_server` process.
  2. **Process-Level**: Advisory `fcntl` locks (`F_RDLCK` / `F_WRLCK`) are acquired on the physical file descriptors of the version and meta files. This ensures that any external tool or process touching `vault_storage/` cannot read half-written files or overwrite records during a push.

### 2. Offloaded Indexing (Separation of Concerns via Message Queue)
- **Problem**: Disk writes to logs and shared manifests are slow. If the main server threads write to logs directly, concurrent client connections could experience delays.
- **Solution**: I detached the indexing logic into a separate background process (`vault_indexer`). When a push succeeds, the server sends a tiny, non-blocking notification to a System V Message Queue. The indexer blocks on `msgrcv()`, picks up messages asynchronously, updates the live manifest, and appends to the index log.

### 3. Named vs. Unnamed Pipes for Conflicts
- **Problem**: When version mismatches occur, I need to generate a patch diff for the client and log the event.
- **Solution**: I combined two piping patterns:
  - **Unnamed Pipe**: Used when forking the `diff -u` utility. I redirect the child process's stdout to the write end of the unnamed pipe using `dup2()`, allowing the parent server thread to read the diff output directly.
  - **Named Pipe (FIFO)**: Created at `/tmp/vault_conflict_pipe`. The server writes formatted conflict details to the FIFO in a non-blocking mode (`O_NONBLOCK`). A dedicated thread in the server monitors this pipe using `select()` to write conflict logs cleanly without stalling the connection.

### 4. Shared Memory & Named Semaphore
- The live version manifest is kept in System V Shared Memory. Because it is shared between the server (which reads it for the `status` command) and the indexer (which writes to it), I protect manifest modifications using a POSIX named semaphore (`/vault_shm_sem`).

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

## Client User Accounts

| Username | Password    | Role        | Capabilities                          |
|----------|-------------|-------------|---------------------------------------|
| alice    | alice123    | owner       | push, pull, list, status, delete, logs|
| bob      | bob123      | contributor | push, pull, list, status              |
| charlie  | charlie123  | contributor | push, pull, list, status              |
| guest1   | guest111    | viewer      | pull, list                            |
| guest2   | guest222    | viewer      | pull, list                            |

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
