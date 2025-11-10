#pragma once

/// @file fl/clockless.h
/// @brief DEPRECATED - BulkClockless API has been removed
///
/// The BulkClockless API (BulkStrip, BulkStripConfig, BulkClockless) has been superseded
/// by the new Channel/ChannelEngine API, which provides better performance and cleaner
/// architecture for managing multiple LED strips.
///
/// **Migration Path:**
/// - Old: `FastLED.addClocklessLeds<WS2812, GRB, RMT>({...})`
/// - New: `FastLED.addLedChannel(Channel::create<ParlioEngine>(config))`
///
/// See `src/fl/channels/` for the new Channel API implementation.
///
/// This file is kept as a placeholder to prevent build errors during migration.
