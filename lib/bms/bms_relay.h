#ifndef BMS_RELAY_H
#define BMS_RELAY_H

#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "packet_tracker.h"

class Packet;

class BmsRelay {
 public:
  /**
   * @brief Function polled for new byte from BMS. Expected to return negative
   * value when there's no data available on the wire.
   */
  typedef std::function<int()> Source;
  /**
   * @brief Function called to send data to the MB.
   */
  typedef std::function<void(uint8_t)> Sink;

  /**
   * @brief Packet callback.
   */
  typedef std::function<void(BmsRelay*, Packet*)> PacketCallback;

  typedef std::function<unsigned long()> MillisProvider;

  BmsRelay(const Source& source, const Sink& sink,
           const MillisProvider& millisProvider);

  /**
   * @brief All of the data ingestion, processing and forwarding is done here.
   * Must be called continuously from arduino loop.
   */
  void loop();

  void addReceivedPacketCallback(const PacketCallback& callback) {
    receivedPacketCallbacks_.push_back(callback);
  }

  void addForwardedPacketCallback(const PacketCallback& callback) {
    forwardedPacketCallbacks_.push_back(callback);
  }

  void setUnknownDataCallback(const Sink& c) { unknownDataCallback_ = c; }


  bool isCharging() { return last_status_byte_ & 0x20; }
  bool isBatteryEmpty() { return last_status_byte_ & 0x4; }
  bool isBatteryTempOutOfRange() {
    // Both these bits throw the same error, one is prob hot and the other is
    // cold.
    return last_status_byte_ & 3;
  }
  bool isBatteryOvercharged() { return last_status_byte_ & 8; }

  const PacketTracker& getPacketTracker() const { return packet_tracker_; }

 private:
  void processNextByte();
  void purgeUnknownData();
  void maybeReplayPackets();
  void ingestPacket(Packet& p);

  std::vector<PacketCallback> receivedPacketCallbacks_;
  std::vector<PacketCallback> forwardedPacketCallbacks_;
  Sink unknownDataCallback_;

  std::vector<uint8_t> sourceBuffer_;


  uint8_t last_status_byte_ = 0;
  const Source source_;
  const Sink sink_;
  const MillisProvider millis_provider_;
  int32_t now_millis_;
  PacketTracker packet_tracker_;
};

#endif  // BMS_RELAY_H