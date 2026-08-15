/**
 *
 * @file realtime.hpp
 * @author Gaspard Kirira
 * @brief Complete public API of the Vix Realtime module.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_REALTIME_HPP
#define VIX_REALTIME_REALTIME_HPP

#include <vix/realtime/api.hpp>
#include <vix/realtime/version.hpp>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/types.hpp>

#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/node_id.hpp>

#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/protocol.hpp>

#include <vix/realtime/room_state.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_factory.hpp>

#include <vix/realtime/event_store.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>

#include <vix/realtime/connection.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_owner.hpp>
#include <vix/realtime/room_directory.hpp>

#include <vix/realtime/presence.hpp>
#include <vix/realtime/presence_store.hpp>
#include <vix/realtime/local_presence_store.hpp>
#include <vix/realtime/distributed_presence.hpp>

#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/server.hpp>

#include <vix/realtime/transport.hpp>
#if VIX_REALTIME_WITH_WEBSOCKET
#include <vix/realtime/websocket_adapter.hpp>
#endif

#include <vix/realtime/session_resume.hpp>

#include <vix/realtime/metrics.hpp>
#include <vix/realtime/health.hpp>

#include <vix/realtime/postgres_event_store.hpp>
#include <vix/realtime/postgres_snapshot_store.hpp>

#endif // VIX_REALTIME_REALTIME_HPP
