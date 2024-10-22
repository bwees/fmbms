#include "bms_relay.h"

#include <cstring>
#include <limits>

#include "packet.h"

namespace {
const uint8_t PREAMBLE[] = {0xFF, 0x55, 0xAA};

unsigned long packetTypeRebroadcastTimeout(int type) {
  if (type == 0 || type == 5) {
    return 500;
  }
  // Packet 11 only appears once on power up.
  if (type == 11) {
    return std::numeric_limits<unsigned long>::max();
  }
  return 3000;
}

}  // namespace

BmsRelay::BmsRelay(const Source& source, const Sink& sink,
                   const MillisProvider& millis)
    : source_(source), sink_(sink), millis_provider_(millis) {
  sourceBuffer_.reserve(64);
}

void BmsRelay::loop() {
  while (true) {
    int byte = source_();
    now_millis_ = millis_provider_();
    if (byte < 0) {
      maybeReplayPackets();
      return;
    }
    sourceBuffer_.push_back(byte);
    processNextByte();
  }
}

void BmsRelay::purgeUnknownData() {
  for (uint8_t b : sourceBuffer_) {
    sink_(b);
  }
  if (unknownDataCallback_) {
    for (uint8_t b : sourceBuffer_) {
      unknownDataCallback_(b);
    }
  }
  sourceBuffer_.clear();
}

// Called with every new byte.
void BmsRelay::processNextByte() {
  // If up to first three bytes of the sourceBuffer don't match expected
  // preamble, flush the data unchanged.
  for (unsigned int i = 0; i < std::min(sizeof(PREAMBLE), sourceBuffer_.size());
       i++) {
    if (sourceBuffer_[i] != PREAMBLE[i]) {
      purgeUnknownData();
      return;
    }
  }
  // Check if we have the message type.
  if (sourceBuffer_.size() < 4) {
    return;
  }
  uint8_t type = sourceBuffer_[3];
  if (type >= sizeof(PACKET_LENGTHS_BY_TYPE) ||
      PACKET_LENGTHS_BY_TYPE[type] < 0) {
    purgeUnknownData();
    return;
  }
  uint8_t len = PACKET_LENGTHS_BY_TYPE[type];
  if (sourceBuffer_.size() < len) {
    return;
  }
  Packet p(sourceBuffer_.data(), len);
  ingestPacket(p);
  sourceBuffer_.clear();
}

void BmsRelay::ingestPacket(Packet& p) {
  for (auto& callback : receivedPacketCallbacks_) {
    callback(this, &p);
  };

  //  Recalculate CRC so that logging callbacks see the correct CRCs
  p.recalculateCrcIfValid();
  if (p.shouldForward()) {
    for (auto& callback : forwardedPacketCallbacks_) {
      callback(this, &p);
    }
  }
  p.recalculateCrcIfValid();
  if (p.shouldForward()) {
    for (int i = 0; i < p.len(); i++) {
      sink_(p.start()[i]);
    }
  }
}