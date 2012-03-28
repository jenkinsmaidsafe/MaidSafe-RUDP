/*******************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of MaidSafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of MaidSafe.net. *
 ******************************************************************************/
// Original author: Christopher M. Kohlhoff (chris at kohlhoff dot com)

#ifndef MAIDSAFE_RUDP_PACKETS_ACK_PACKET_H_
#define MAIDSAFE_RUDP_PACKETS_ACK_PACKET_H_

#include <string>

#include "boost/asio/buffer.hpp"
#include "boost/asio/ip/address.hpp"
#include "boost/cstdint.hpp"
#include "boost/system/error_code.hpp"
#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/rudp_control_packet.h"

namespace maidsafe {

namespace transport {

class RudpAckPacket : public RudpControlPacket {
 public:
  enum { kPacketSize = RudpControlPacket::kHeaderSize + 4 };
  enum { kOptionalPacketSize = RudpControlPacket::kHeaderSize + 24 };
  enum { kPacketType = 2 };

  RudpAckPacket();
  virtual ~RudpAckPacket() {}

  boost::uint32_t AckSequenceNumber() const;
  void SetAckSequenceNumber(boost::uint32_t n);

  boost::uint32_t PacketSequenceNumber() const;
  void SetPacketSequenceNumber(boost::uint32_t n);

  bool HasOptionalFields() const;
  void SetHasOptionalFields(bool b);

  // The following fields are optional in the encoded packet.

  boost::uint32_t RoundTripTime() const;
  void SetRoundTripTime(boost::uint32_t n);

  boost::uint32_t RoundTripTimeVariance() const;
  void SetRoundTripTimeVariance(boost::uint32_t n);

  boost::uint32_t AvailableBufferSize() const;
  void SetAvailableBufferSize(boost::uint32_t n);

  boost::uint32_t PacketsReceivingRate() const;
  void SetPacketsReceivingRate(boost::uint32_t n);

  boost::uint32_t EstimatedLinkCapacity() const;
  void SetEstimatedLinkCapacity(boost::uint32_t n);

  // End of optional fields.

  static bool IsValid(const boost::asio::const_buffer &buffer);
  bool Decode(const boost::asio::const_buffer &buffer);
  size_t Encode(const boost::asio::mutable_buffer &buffer) const;

 private:
  boost::uint32_t packet_sequence_number_;
  bool has_optional_fields_;
  boost::uint32_t round_trip_time_;
  boost::uint32_t round_trip_time_variance_;
  boost::uint32_t available_buffer_size_;
  boost::uint32_t packets_receiving_rate_;
  boost::uint32_t estimated_link_capacity_;
};

}  // namespace transport

}  // namespace maidsafe

#endif  // MAIDSAFE_RUDP_PACKETS_ACK_PACKET_H_