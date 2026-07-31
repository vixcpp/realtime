# Vix v2.8.0

## Added

### Realtime module

Added the new `vix::realtime` module for building stateful realtime systems in C++.

The module provides a transport-independent runtime based on authoritative rooms, serialized commands, deterministic events, logical sessions, snapshots, and event replay.

### Stateful rooms

Applications can now define authoritative room state and application-specific command handlers.

Each room owns:

- one authoritative state;
- one command handler;
- one serialized command execution path;
- logical session memberships;
- the current room version;
- the latest persisted event position;
- snapshot scheduling and restoration state.

Commands are processed serially to preserve deterministic room behavior.

### Commands and events

Added application-defined room commands and persisted room events.

Commands support:

- room and session identifiers;
- typed payloads;
- request identifiers;
- correlation identifiers;
- expected room versions;
- metadata;
- validation.

Accepted commands can produce one or more room events.

Events are assigned authoritative room versions and event identifiers before being persisted and applied to room state.

### Event audiences

Added event audience support for controlling delivery visibility.

Supported audiences include:

- the entire room;
- the sending session;
- every session except the sender;
- selected sessions.

### Snapshots and replay

Added room snapshots for compact state persistence and faster restoration.

Rooms can now:

- create snapshots automatically after a configured number of events;
- create a final snapshot when closing;
- retain a configurable number of recent snapshots;
- restore the latest snapshot;
- replay events persisted after the snapshot;
- restore the entire event history when no snapshot exists.

Room restoration preserves deterministic state, room version, and event position.

### Logical sessions

Added transport-independent logical sessions.

Sessions support:

- stable session identifiers;
- identity and metadata;
- connection attachment and detachment;
- room membership tracking;
- resume tokens;
- per-room event cursors;
- connection replacement during reconnection.

A logical session can survive the lifetime of an individual network connection.

### Session reconnection

Added session recovery using resume tokens.

Clients can reconnect and continue from their last acknowledged room event.

Recovery supports:

- resume token validation;
- token rotation;
- token revocation;
- configurable recovery windows;
- connection replacement;
- replay of missing events;
- independent replay positions for multiple rooms.

### Snapshot fallback during reconnection

Added snapshot fallback when a client is too far behind for a bounded event replay.

The recovery flow can now:

- send the latest room snapshot;
- continue replay from the snapshot event position;
- deliver only the remaining events;
- restore the client to the latest room version.

### Presence

Added local presence tracking for logical sessions.

Presence records support:

- room and session identifiers;
- identity;
- node ownership;
- connection identifiers;
- present, detached, and left states;
- heartbeat timestamps;
- metadata;
- stale presence cleanup.

### Room manager

Added `RoomManager` for coordinating rooms, sessions, factories, persistence stores, and routing.

The manager supports:

- room factory registration;
- room opening and closing;
- room lookup;
- session creation and lookup;
- connection attachment and detachment;
- room joins and leaves;
- synchronous and queued command execution;
- presence lookup;
- configured room and session limits;
- inactive room cleanup;
- runtime shutdown.

### Room directory and ownership

Added room directory and ownership abstractions for locating active rooms and preparing future multi-process routing.

Rooms can be associated with the node currently responsible for their authoritative execution.

### Persistence interfaces

Added interchangeable persistence interfaces for realtime events, snapshots, and presence.

The initial implementation includes:

- `MemoryEventStore`;
- `MemorySnapshotStore`;
- local in-memory presence storage.

The persistence model is designed to support durable external stores without changing application room logic.

### PostgreSQL persistence

Added optional PostgreSQL-backed realtime persistence support.

When enabled, PostgreSQL stores can persist room events and snapshots for durable recovery across process restarts.

PostgreSQL support remains optional and does not affect the transport-independent realtime core.

### WebSocket adapter

Added an optional adapter between `vix::realtime` and `vix::websocket`.

The adapter:

- wraps WebSocket sessions as realtime connections;
- parses realtime protocol envelopes;
- forwards connection lifecycle events;
- delivers room events;
- handles protocol failures;
- enforces configured message-size limits;
- supports configurable connection identifiers;
- remains independent from the authoritative room runtime.

The realtime core can be used without WebSocket.

### Realtime protocol

Added a versioned realtime protocol envelope for client and server communication.

Supported protocol flows include:

- session opening;
- session resumption;
- room joining;
- room leaving;
- room commands;
- room acknowledgements;
- room snapshots;
- room events;
- replay completion;
- command acceptance and rejection;
- presence updates;
- ping and pong;
- protocol errors;
- server draining notifications.

### Public API

Added the stable umbrella header:

```cpp
#include <vix/realtime.hpp>
```

Advanced APIs remain available through module-specific headers under:

```cpp
#include <vix/realtime/...>
```

### Examples

Added complete realtime examples:

- `counter` demonstrates commands, events, snapshots, restoration, and continued room versions;
- `shared_room` demonstrates multiple sessions sharing one authoritative state;
- `chat` demonstrates persisted messages, room membership, history, snapshots, and restoration;
- `reconnect` demonstrates acknowledged cursors, missing-event replay, and snapshot fallback.

### Tests

Added tests covering:

- identifiers and version types;
- commands and events;
- protocol parsing and validation;
- state application;
- memory event and snapshot stores;
- snapshot policies;
- sessions and resume tokens;
- presence;
- room lifecycle;
- command backpressure;
- room failures;
- room management;
- room ownership and directory behavior;
- WebSocket transport integration;
- reconnection replay;
- snapshot fallback;
- complete realtime session flows;
- deterministic snapshot and event restoration.
