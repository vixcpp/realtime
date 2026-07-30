# Vix Realtime

Stateful, transport-independent realtime rooms for Vix.cpp.

`vix::realtime` provides the runtime foundations required to build collaborative applications, multiplayer services, live dashboards, chat systems, shared workspaces, presence systems, and other stateful realtime services in modern C++.

The module separates authoritative room logic from networking:

- commands describe client intentions;
- room handlers validate commands and produce events;
- events are persisted before being applied;
- room state is reconstructed from snapshots and event replay;
- sessions survive temporary transport disconnections;
- presence tracks logical room membership;
- transports deliver protocol envelopes without owning application logic.

## Status

Current module version:

```text
0.1.0
```

The API is under active development. Public headers outside `internal/` are intended to form the stable module surface.

## Requirements

- C++20
- CMake
- Vix.cpp core
- Vix JSON
- Vix WebSocket for the optional WebSocket adapter
- PostgreSQL and libpq for the optional PostgreSQL stores

## Public entry point

Include the complete public API with:

```cpp
#include <vix/realtime.hpp>
```

Advanced users may include individual module headers:

```cpp
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session_resume.hpp>
```

Headers under `vix/realtime/internal/` are implementation details and are not part of the stable public API.

## Core model

A Realtime room follows an event-driven execution model:

```text
RoomCommand
    |
    v
RoomHandler
    |
    v
CommandResult
    |
    v
RoomEvent persistence
    |
    v
RoomState::apply()
    |
    v
Event dispatch
```

The authoritative state changes only through persisted room events.

This provides:

- deterministic state transitions;
- replayable room history;
- storage-independent room logic;
- recoverable state after process restarts;
- transport-independent command handling;
- explicit event audiences;
- optimistic room-version checks.

## Main components

### Room identifiers and stream positions

The module uses strongly typed identifiers:

```cpp
vix::realtime::RoomId roomId{"workspace/main"};
vix::realtime::SessionId sessionId{"session-42"};
vix::realtime::NodeId nodeId{"node-1"};

vix::realtime::RoomVersion version{0};
vix::realtime::EventId eventId{0};
```

`RoomVersion` identifies the logical version of room state.

`EventId` identifies the position of an event in one room event stream.

### Commands

A `RoomCommand` represents one client or server intention:

```cpp
vix::realtime::JsonObject payload;
payload.set_string("message", "Hello");

vix::realtime::RoomCommand command{
    vix::realtime::RoomId{"chat/general"},
    vix::realtime::SessionId{"session-42"},
    "message.send",
    std::move(payload)};

command.set_request_id("request-1");
command.set_correlation_id("conversation-9");
```

Commands may include an expected room version:

```cpp
command.set_expected_version(
    vix::realtime::RoomVersion{12});
```

The expected version can be used for optimistic concurrency validation.

### Events

A `RoomEvent` describes an authoritative state transition:

```cpp
vix::realtime::JsonObject payload;
payload.set_string("message", "Hello");

vix::realtime::RoomEvent event{
    vix::realtime::RoomId{"chat/general"},
    "message.sent",
    std::move(payload),
    vix::realtime::EventAudience::Room};
```

Supported audiences are:

| Audience   | Delivery                             |
| ---------- | ------------------------------------ |
| `Room`     | Every session in the room            |
| `Sender`   | Only the source session              |
| `Others`   | Every room session except the source |
| `Session`  | One explicit target session          |
| `Internal` | No transport delivery                |

Session-targeted events require a target session:

```cpp
event.set_target_session(
    vix::realtime::SessionId{"session-84"});

event.set_audience(
    vix::realtime::EventAudience::Session);
```

### Command results

Room handlers return a `CommandResult`:

```cpp
return vix::realtime::CommandResult::accepted(
    {std::move(event)});
```

A command may be:

- accepted;
- rejected;
- ignored.

Example rejection:

```cpp
return vix::realtime::CommandResult::rejected(
    vix::realtime::ErrorCode::Unauthorized,
    "session cannot modify this room");
```

## Defining room state

Application state derives from `RoomState`.

```cpp
#include <memory>
#include <string>

#include <vix/realtime.hpp>

class ChatState final
    : public vix::realtime::RoomState
{
public:
  [[nodiscard]] vix::realtime::SchemaVersion
  schema_version() const noexcept override
  {
    return 1;
  }

  void apply(
      const vix::realtime::RoomEvent &event) override
  {
    if (event.type() == "message.sent")
    {
      ++messageCount_;
    }
  }

  [[nodiscard]] vix::realtime::JsonObject
  serialize() const override
  {
    vix::realtime::JsonObject state;

    state.set_i64(
        "message_count",
        messageCount_);

    return state;
  }

  void restore(
      const vix::realtime::JsonObject &state,
      vix::realtime::SchemaVersion schemaVersion) override
  {
    if (schemaVersion != 1)
    {
      throw vix::realtime::Error{
          vix::realtime::ErrorCode::CorruptedState,
          "unsupported chat state schema"};
    }

    /*
     * Read the application fields using the Vix JSON accessors used by the
     * surrounding application.
     */
  }

  [[nodiscard]] std::unique_ptr<vix::realtime::RoomState>
  clone() const override
  {
    return std::make_unique<ChatState>(*this);
  }

private:
  std::int64_t messageCount_{0};
};
```

`clone()` is used to isolate command execution and replay work from the current authoritative state.

## Defining a room handler

A handler validates commands and emits events.

```cpp
class ChatHandler final
    : public vix::realtime::RoomHandler
{
public:
  [[nodiscard]] vix::realtime::CommandResult
  handle_command(
      const vix::realtime::RoomCommand &command,
      const vix::realtime::RoomState &state,
      const vix::realtime::RoomContext &context) override
  {
    static_cast<void>(state);
    static_cast<void>(context);

    if (command.type() != "message.send")
    {
      return vix::realtime::CommandResult::ignored();
    }

    vix::realtime::RoomEvent event{
        command.room_id(),
        "message.sent",
        command.payload(),
        vix::realtime::EventAudience::Room};

    event.set_source_session(
        command.session_id());

    event.set_request_id(
        command.request_id());

    event.set_correlation_id(
        command.correlation_id());

    return vix::realtime::CommandResult::accepted(
        {std::move(event)});
  }
};
```

Lifecycle hooks may also emit events:

```cpp
vix::realtime::CommandResult on_open(
    const vix::realtime::RoomState &state,
    const vix::realtime::RoomContext &context) override;

vix::realtime::CommandResult on_join(
    const vix::realtime::SessionId &sessionId,
    const vix::realtime::RoomState &state,
    const vix::realtime::RoomContext &context) override;

vix::realtime::CommandResult on_leave(
    const vix::realtime::SessionId &sessionId,
    const vix::realtime::RoomState &state,
    const vix::realtime::RoomContext &context) override;

vix::realtime::CommandResult on_close(
    const vix::realtime::RoomState &state,
    const vix::realtime::RoomContext &context) override;
```

## Defining a room factory

A `RoomFactory` creates a state and handler for one room type.

```cpp
class ChatFactory final
    : public vix::realtime::RoomFactory
{
public:
  [[nodiscard]] std::string_view
  room_type() const noexcept override
  {
    return "chat";
  }

  [[nodiscard]] vix::realtime::RoomStatePtr
  create_state(
      const vix::realtime::RoomId &roomId) const override
  {
    static_cast<void>(roomId);

    return std::make_unique<ChatState>();
  }

  [[nodiscard]] vix::realtime::RoomHandlerPtr
  create_handler(
      const vix::realtime::RoomId &roomId) const override
  {
    static_cast<void>(roomId);

    return std::make_unique<ChatHandler>();
  }
};
```

## Creating a server

`Server` is the transport-independent runtime facade.

```cpp
#include <memory>

#include <vix/realtime.hpp>

int main()
{
  vix::realtime::Config config;

  config.maxActiveRooms = 1000;
  config.maxSessions = 10000;
  config.maxSessionsPerRoom = 256;
  config.enableSessionResume = true;
  config.enablePresence = true;

  auto server =
      std::make_shared<vix::realtime::Server>(
          vix::realtime::NodeId{"node-1"},
          config);

  server->register_factory(
      std::make_shared<ChatFactory>());

  server->start();

  auto room =
      server->open_room(
          vix::realtime::RoomId{"chat/general"},
          "chat");

  return 0;
}
```

`Server` does not open sockets. A transport adapter forwards connections and protocol envelopes to the runtime.

## Sessions

A session is a logical client identity that may survive a temporary transport disconnection.

```cpp
auto session =
    server->create_session(
        vix::realtime::SessionId{"session-42"},
        "user-17");
```

A session may:

- attach to one active connection;
- join multiple rooms;
- detach without losing memberships;
- resume within a configured window;
- hold application metadata;
- be permanently closed.

### Joining a room

```cpp
auto result =
    server->join_room(
        vix::realtime::SessionId{"session-42"},
        vix::realtime::RoomId{"chat/general"});

if (result.is_rejected())
{
  // Handle the lifecycle rejection.
}
```

### Leaving a room

```cpp
server->leave_room(
    vix::realtime::SessionId{"session-42"},
    vix::realtime::RoomId{"chat/general"});
```

## Executing commands

Commands may be executed immediately:

```cpp
vix::realtime::RoomCommand command{
    vix::realtime::RoomId{"chat/general"},
    vix::realtime::SessionId{"session-42"},
    "message.send",
    payload};

auto result =
    server->execute(command);
```

They may also be inserted into a room queue:

```cpp
const auto queueStatus =
    server->enqueue(
        std::move(command));

if (queueStatus ==
    vix::realtime::internal::CommandQueueStatus::Success)
{
  server->process_next(
      vix::realtime::RoomId{"chat/general"});
}
```

Queued execution preserves command ordering inside one room.

## Event persistence

`EventStore` is the authoritative room event persistence contract.

Provided implementations:

- `MemoryEventStore`
- `PostgresEventStore`

### In-memory event store

```cpp
auto eventStore =
    std::make_shared<
        vix::realtime::MemoryEventStore>();
```

The in-memory store is useful for:

- tests;
- examples;
- local development;
- ephemeral applications.

It does not survive process restarts.

### PostgreSQL event store

```cpp
vix::realtime::PostgresEventStoreOptions options;

options.connectionString =
    "host=127.0.0.1 port=5432 dbname=vix user=vix password=secret";

options.schema = "public";
options.table = "vix_realtime_events";
options.createTableIfMissing = true;

auto eventStore =
    std::make_shared<
        vix::realtime::PostgresEventStore>(
            std::move(options));
```

The PostgreSQL event store:

- assigns event IDs transactionally per room;
- validates contiguous room versions;
- validates contiguous event IDs during replay;
- uses room-specific PostgreSQL advisory transaction locks;
- supports atomic event batches;
- stores payload and metadata as JSONB.

PostgreSQL support must be enabled when building the module.

## Snapshots

Snapshots reduce the number of events required to restore a room.

Provided implementations:

- `MemorySnapshotStore`
- `PostgresSnapshotStore`

### Snapshot configuration

```cpp
vix::realtime::Config config;

config.snapshotEveryEvents = 100;
config.snapshotsToKeep = 3;
config.snapshotOnRoomClose = true;
config.restoreRoomsOnOpen = true;
```

### PostgreSQL snapshot store

```cpp
vix::realtime::PostgresSnapshotStoreOptions options;

options.connectionString =
    "host=127.0.0.1 port=5432 dbname=vix user=vix password=secret";

options.schema = "public";
options.table = "vix_realtime_snapshots";
options.createTableIfMissing = true;

auto snapshotStore =
    std::make_shared<
        vix::realtime::PostgresSnapshotStore>(
            std::move(options));
```

Snapshots are uniquely identified by:

```text
room_id + room_version
```

Replacing an existing snapshot version requires the same `last_event_id`.

## Replay

`ReplayEngine` reconstructs room state from a snapshot and subsequent events.

```cpp
auto replayEngine =
    vix::realtime::internal::ReplayEngine::from_config(
        config,
        eventStore,
        snapshotStore);

auto replayResult =
    replayEngine.restore(
        roomId,
        roomState);
```

Replay validates:

- snapshot room identity;
- snapshot stream position;
- contiguous room versions;
- contiguous event IDs;
- event count limit;
- serialized byte limit;
- replay timeout;
- event application failures.

Default limits are configured through:

```cpp
config.maxReplayEvents = 1000;
config.maxReplayBytes = 4U * 1024U * 1024U;
config.replayTimeout = std::chrono::milliseconds{5000};
```

## Presence

Presence represents ephemeral logical membership in rooms.

```cpp
auto presenceStore =
    std::make_shared<
        vix::realtime::LocalPresenceStore>();
```

A presence record contains:

- room ID;
- session ID;
- optional node ID;
- optional connection ID;
- identity;
- joined timestamp;
- latest activity timestamp;
- detached or left timestamp;
- metadata.

Presence may be:

- present;
- detached;
- left.

Presence is not authoritative room state and should not replace persisted events.

## Distributed presence

`DistributedPresence` defines the contract for a presence store shared by multiple runtime nodes.

A distributed implementation must provide:

- the full `PresenceStore` API;
- local node identity;
- node heartbeats;
- active-node queries;
- stale-node pruning;
- per-node presence cleanup;
- backend health reporting;
- backend connectivity checks.

The module does not impose a specific distributed backend.

Possible implementations include:

- PostgreSQL;
- Redis;
- a shared key-value store;
- a message broker;
- a dedicated coordination service.

## Room ownership

`RoomDirectory` tracks the runtime node responsible for one room.

```cpp
auto directory =
    std::make_shared<
        vix::realtime::RoomDirectory>();
```

Room ownership supports:

- monotonically increasing generations;
- optional leases;
- renewal;
- transfer;
- release;
- stale-owner pruning;
- local ownership checks.

A generation prevents an older owner from reclaiming authority after a newer ownership claim has been created.

`RoomDirectory` is process-local. Distributed deployments should provide shared routing or coordination around this contract.

## Session resumption

`SessionResume` issues and validates opaque session credentials.

```cpp
auto resume =
    std::make_shared<
        vix::realtime::SessionResume>(
            server->manager());

const auto token =
    resume->issue(
        vix::realtime::SessionId{"session-42"});
```

After a transport disconnection:

```cpp
auto result =
    resume->resume(
        vix::realtime::SessionId{"session-42"},
        token,
        replacementConnection);
```

Successful resumption may rotate the token:

```cpp
const auto nextToken =
    result.resumeToken;
```

Resume validation requires:

- session resumption enabled;
- an existing logical session;
- a matching opaque token;
- a detached session;
- a non-expired resume window;
- an open replacement connection.

Default resume configuration:

```cpp
config.enableSessionResume = true;
config.sessionResumeWindow =
    std::chrono::seconds{120};
```

Tokens use URL-safe, unpadded Base64 encoding with configurable entropy.

## Protocol

The Realtime protocol uses structured envelopes.

Envelope kinds are:

- request;
- response;
- event;
- error;
- snapshot;
- control.

Serialization:

```cpp
const std::string text =
    vix::realtime::protocol::serialize(
        envelope);
```

Parsing:

```cpp
const auto envelope =
    vix::realtime::protocol::parse(
        text);
```

Current protocol version:

```text
1.0
```

Protocol envelopes may contain:

- message kind;
- protocol version;
- request ID;
- correlation ID;
- room ID;
- session ID;
- command or event type;
- room version;
- event ID;
- schema version;
- payload;
- metadata;
- error information.

## Transport abstraction

`Transport` converts transport-specific activity into Realtime connections and protocol envelopes.

```cpp
vix::realtime::TransportHandlers handlers;

handlers.onOpen =
    [](vix::realtime::ConnectionPtr connection)
    {
      // Associate the connection with a logical session.
    };

handlers.onEnvelope =
    [](vix::realtime::ConnectionPtr connection,
       const vix::realtime::protocol::Envelope &envelope)
    {
      // Route the envelope to application runtime logic.
    };

handlers.onClose =
    [](vix::realtime::ConnectionPtr connection)
    {
      // Detach the logical session.
    };

handlers.onError =
    [](vix::realtime::ConnectionPtr connection,
       vix::realtime::ErrorCode code,
       const std::string &message)
    {
      // Record or report the transport error.
    };
```

The abstraction allows Realtime to support WebSocket, TCP, local IPC, tests, or custom transports without changing room logic.

## WebSocket adapter

`WebSocketAdapter` bridges Vix WebSocket sessions with the Realtime transport contract.

```cpp
vix::websocket::Server websocketServer;

auto adapter =
    std::make_shared<
        vix::realtime::WebSocketAdapter>(
            websocketServer);

adapter->set_handlers(
    std::move(handlers));

adapter->attach();
```

The adapter:

- wraps WebSocket sessions as Realtime connections;
- generates process-local connection IDs;
- parses incoming protocol envelopes;
- serializes outgoing envelopes;
- enforces an optional message-size limit;
- reports protocol and transport errors;
- may close invalid connections.

The adapter does not start or stop the underlying WebSocket server.

## Metrics

`Metrics` provides thread-safe observational counters and gauges.

```cpp
auto metrics =
    std::make_shared<
        vix::realtime::Metrics>();

metrics->record_room_opened();
metrics->record_session_created();
metrics->record_events_persisted(2);

const auto snapshot =
    metrics->snapshot();
```

Available metrics include:

- active rooms;
- active sessions;
- attached connections;
- queued commands;
- active presence;
- opened and closed rooms;
- created and closed sessions;
- processed, accepted, rejected, and ignored commands;
- persisted events;
- event dispatch recipients and failures;
- snapshots created and restored;
- replay operations, events, bytes, and duration;
- session resume attempts;
- transport messages and bytes;
- protocol errors;
- runtime errors.

Metrics use relaxed atomics and do not participate in authoritative runtime behavior.

## Health monitoring

`HealthMonitor` creates a point-in-time runtime report.

```cpp
auto monitor =
    std::make_shared<
        vix::realtime::HealthMonitor>(
            server,
            metrics);

const auto report =
    monitor->check();
```

Health states are:

- `Healthy`
- `Degraded`
- `Unhealthy`
- `Stopped`

The report inspects:

- server lifecycle;
- event-store availability;
- snapshot-store availability;
- presence-store availability;
- room-directory availability;
- room lifecycle states;
- local room ownership;
- command queue depth;
- connected and detached sessions;
- retained closed sessions;
- presence count;
- runtime and protocol error counters.

Example:

```cpp
if (!report.operational())
{
  for (const auto &issue : report.issues)
  {
    // Report the issue.
  }
}
```

## Configuration

Default runtime configuration:

| Option                      |              Default |
| --------------------------- | -------------------: |
| `maxActiveRooms`            |               `1000` |
| `maxSessions`               |              `10000` |
| `maxSessionsPerRoom`        |                `256` |
| `maxRoomsPerSession`        |                 `32` |
| `maxPendingCommandsPerRoom` |               `1024` |
| `maxPayloadSize`            |             `64 KiB` |
| `maxReplayEvents`           |               `1000` |
| `maxReplayBytes`            |              `4 MiB` |
| `maxResumeRooms`            |                 `32` |
| `snapshotEveryEvents`       |                `100` |
| `snapshotsToKeep`           |                  `3` |
| `roomIdleTimeout`           |        `300 seconds` |
| `commandTimeout`            |  `5000 milliseconds` |
| `roomOpenTimeout`           | `10000 milliseconds` |
| `sessionResumeWindow`       |        `120 seconds` |
| `presenceHeartbeatInterval` |         `30 seconds` |
| `presenceTimeout`           |         `90 seconds` |
| `replayTimeout`             |  `5000 milliseconds` |
| `snapshotOnRoomClose`       |               `true` |
| `restoreRoomsOnOpen`        |               `true` |
| `enableSessionResume`       |               `true` |
| `enablePresence`            |               `true` |

Validate configuration before constructing custom runtime components:

```cpp
vix::realtime::Config config;
config.validate();
```

## Error handling

Realtime operations use `vix::realtime::Error`.

```cpp
try
{
  server->open_room(
      vix::realtime::RoomId{"chat/general"},
      "chat");
}
catch (const vix::realtime::Error &error)
{
  const auto code =
      error.code();

  const auto name =
      vix::realtime::to_string(code);
}
```

Important error categories include:

- invalid configuration;
- missing dependencies;
- room lifecycle failures;
- session expiration;
- invalid resume tokens;
- command rejection;
- queue saturation;
- payload limits;
- event-store failures;
- snapshot-store failures;
- replay failures;
- transport failures;
- protocol errors;
- timeouts;
- corrupted state.

## Runtime guarantees

### Per-room ordering

Commands processed by one room are serialized through the room command queue and room execution lock.

### Persist before apply

Produced events are persisted before they are applied to authoritative room state.

### Persist before dispatch

Events are dispatched only after successful persistence and state application.

### Atomic event batches

A command producing multiple events commits them as one store batch where the selected event-store implementation supports atomic batches.

### Replay validation

State restoration rejects gaps or inconsistencies in room versions and event identifiers.

### Session continuity

Logical sessions may remain alive after a transport connection disappears.

### Transport independence

Application state and handlers do not depend on WebSocket types.

## Deployment model

A single-process deployment may use:

```text
RoomManager
MemoryEventStore
MemorySnapshotStore
LocalPresenceStore
RoomDirectory
WebSocketAdapter
```

A durable single-node deployment may use:

```text
RoomManager
PostgresEventStore
PostgresSnapshotStore
LocalPresenceStore
RoomDirectory
WebSocketAdapter
```

A multi-node deployment additionally requires shared coordination for:

- room ownership;
- command routing;
- distributed presence;
- connection-to-session routing;
- event delivery across nodes.

`DistributedPresence` defines the shared-presence contract, while `RoomOwner` and `RoomDirectory` define the ownership model used by future distributed coordination implementations.

## Suggested application architecture

```text
HTTP / WebSocket runtime
          |
          v
   WebSocketAdapter
          |
          v
        Server
          |
          v
     RoomManager
      /   |    \
     /    |     \
 Rooms  Sessions  Presence
   |        |
   v        v
Handlers  Connections
   |
   v
EventStore + SnapshotStore
```

Application code should place business rules in:

- `RoomState`;
- `RoomHandler`;
- `RoomFactory`.

Transport callbacks should remain focused on:

- authentication;
- session creation;
- session resumption;
- envelope routing;
- connection detachment.

## Testing

The module is designed so most application logic can be tested without sockets.

Recommended test layers:

1. Test `RoomState::apply()` with explicit events.
2. Test `RoomHandler::handle_command()` with explicit state and context.
3. Test `Room` with memory stores.
4. Test `RoomManager` session and membership behavior.
5. Test protocol serialization and parsing.
6. Test transport adapters with connection doubles.
7. Run PostgreSQL integration tests separately.

Example memory-backed setup:

```cpp
auto eventStore =
    std::make_shared<
        vix::realtime::MemoryEventStore>();

auto snapshotStore =
    std::make_shared<
        vix::realtime::MemorySnapshotStore>();

auto presenceStore =
    std::make_shared<
        vix::realtime::LocalPresenceStore>();

auto directory =
    std::make_shared<
        vix::realtime::RoomDirectory>();
```

## License

Vix Realtime is distributed under the MIT License.

Copyright 2026, Gaspard Kirira.

Vix.cpp
https://github.com/vixcpp/vix
