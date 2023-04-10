#ifndef __APPLE__
#include <sys/file.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "common/util.h"
#include "common/timing.h"
#include "common/swaglog.h"
#include "panda/board/comms_definitions.h"
#include "selfdrive/boardd/panda_comms.h"


#define SPI_SYNC 0x5AU
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU
#define SPI_CHECKSUM_START 0xABU

struct __attribute__((packed)) spi_header {
  uint8_t sync;
  uint8_t endpoint;
  uint16_t tx_len;
  uint16_t max_rx_len;
};

const int SPI_MAX_RETRIES = 5;
const int SPI_ACK_TIMEOUT = 50; // milliseconds
const std::string SPI_DEVICE = "/dev/spidev0.0";

class LockEx {
public:
  LockEx(int fd, std::recursive_mutex &m) : fd(fd), m(m) {
    m.lock();
    flock(fd, LOCK_EX);
  };

  ~LockEx() {
    m.unlock();
    flock(fd, LOCK_UN);
  }

private:
  int fd;
  std::recursive_mutex &m;
};


PandaSpiHandle::PandaSpiHandle(std::string serial) : PandaCommsHandle(serial) {
  int ret;
  const int uid_len = 12;
  uint8_t uid[uid_len] = {0};

  uint32_t spi_mode = SPI_MODE_0;
  uint8_t spi_bits_per_word = 8;

  // 50MHz is the max of the 845. note that some older
  // revs of the comma three may not support this speed
  uint32_t spi_speed = 50000000;

  spi_fd = open(SPI_DEVICE.c_str(), O_RDWR);
  if (spi_fd < 0) {
    LOGE("failed opening SPI device %d", spi_fd);
    goto fail;
  }

  // SPI settings
  ret = util::safe_ioctl(spi_fd, SPI_IOC_WR_MODE, &spi_mode);
  if (ret < 0) {
    LOGE("failed setting SPI mode %d", ret);
    goto fail;
  }

  ret = util::safe_ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
  if (ret < 0) {
    LOGE("failed setting SPI speed");
    goto fail;
  }

  ret = util::safe_ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits_per_word);
  if (ret < 0) {
    LOGE("failed setting SPI bits per word");
    goto fail;
  }

  // get hw UID/serial
  ret = control_read(0xc3, 0, 0, uid, uid_len);
  if (ret == uid_len) {
    std::stringstream stream;
    for (int i = 0; i < uid_len; i++) {
      stream << std::hex << std::setw(2) << std::setfill('0') << int(uid[i]);
    }
    hw_serial = stream.str();
  } else {
    LOGD("failed to get serial %d", ret);
    goto fail;
  }

  if (!serial.empty() && (serial != hw_serial)) {
    goto fail;
  }

  return;

fail:
  cleanup();
  throw std::runtime_error("Error connecting to panda");
}

PandaSpiHandle::~PandaSpiHandle() {
  std::lock_guard lk(hw_lock);
  cleanup();
}

void PandaSpiHandle::cleanup() {
  if (spi_fd != -1) {
    close(spi_fd);
    spi_fd = -1;
  }
}



int PandaSpiHandle::control_write(uint8_t request, uint16_t param1, uint16_t param2, unsigned int timeout) {
  LockEx lock(spi_fd, hw_lock);
  ControlPacket_t packet = {
    .request = request,
    .param1 = param1,
    .param2 = param2,
    .length = 0
  };
  return spi_transfer_retry(0, (uint8_t *) &packet, sizeof(packet), NULL, 0);
}

int PandaSpiHandle::control_read(uint8_t request, uint16_t param1, uint16_t param2, unsigned char *data, uint16_t length, unsigned int timeout) {
  LockEx lock(spi_fd, hw_lock);
  ControlPacket_t packet = {
    .request = request,
    .param1 = param1,
    .param2 = param2,
    .length = length
  };
  return spi_transfer_retry(0, (uint8_t *) &packet, sizeof(packet), data, length);
}

int PandaSpiHandle::bulk_write(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {
  LockEx lock(spi_fd, hw_lock);
  return bulk_transfer(endpoint, data, length, NULL, 0);
}
int PandaSpiHandle::bulk_read(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {
  LockEx lock(spi_fd, hw_lock);
  return bulk_transfer(endpoint, NULL, 0, data, length);
}

int PandaSpiHandle::bulk_transfer(uint8_t endpoint, uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len) {
  const int xfer_size = 0x40 * 15;

  int ret = 0;
  uint16_t length = (tx_data != NULL) ? tx_len : rx_len;
  for (int i = 0; i < (int)std::ceil((float)length / xfer_size); i++) {
    int d;
    if (tx_data != NULL) {
      int len = std::min(xfer_size, tx_len - (xfer_size * i));
      d = spi_transfer_retry(endpoint, tx_data + (xfer_size * i), len, NULL, 0);
    } else {
      uint16_t to_read = std::min(xfer_size, rx_len - ret);
      d = spi_transfer_retry(endpoint, NULL, 0, rx_data + (xfer_size * i), to_read);
    }

    if (d < 0) {
      LOGE("SPI: bulk transfer failed with %d", d);
      comms_healthy = false;
      return -1;
    }

    ret += d;
    if ((rx_data != NULL) && d < xfer_size) {
      break;
    }
  }

  return ret;
}

std::vector<std::string> PandaSpiHandle::list() {
  try {
    PandaSpiHandle sh("");
    return {sh.hw_serial};
  } catch (std::exception &e) {
    // no panda on SPI
  }
  return {};
}

void add_checksum(uint8_t *data, int data_len) {
  data[data_len] = SPI_CHECKSUM_START;
  for (int i=0; i < data_len; i++) {
    data[data_len] ^= data[i];
  }
}

bool check_checksum(uint8_t *data, int data_len) {
  uint8_t checksum = SPI_CHECKSUM_START;
  for (uint16_t i = 0U; i < data_len; i++) {
    checksum ^= data[i];
  }
  return checksum == 0U;
}


int PandaSpiHandle::spi_transfer_retry(uint8_t endpoint, uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t max_rx_len) {
  int ret;

  int count = SPI_MAX_RETRIES;
  do {
    // TODO: handle error
    ret = spi_transfer(endpoint, tx_data, tx_len, rx_data, max_rx_len);
    count--;
  } while (ret < 0 && connected && count > 0);

  return ret;
}

int PandaSpiHandle::wait_for_ack(spi_ioc_transfer &transfer, uint8_t ack) {
  double start_millis = millis_since_boot();
  while (true) {
    int ret = util::safe_ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
      LOGE("SPI: failed to send ACK request");
      return ret;
    }

    if (rx_buf[0] == ack) {
      break;
    } else if (rx_buf[0] == SPI_NACK) {
      LOGW("SPI: got NACK");
      return -1;
    }

    // handle timeout
    if (millis_since_boot() - start_millis > SPI_ACK_TIMEOUT) {
      LOGD("SPI: timed out waiting for ACK");
      return -1;
    }
  }

  return 0;
}

int PandaSpiHandle::spi_transfer(uint8_t endpoint, uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t max_rx_len) {
  int ret;
  uint16_t rx_data_len;

  // needs to be less, since we need to have space for the checksum
  assert(tx_len < SPI_BUF_SIZE);
  assert(max_rx_len < SPI_BUF_SIZE);

  spi_header header = {
    .sync = SPI_SYNC,
    .endpoint = endpoint,
    .tx_len = tx_len,
    .max_rx_len = max_rx_len
  };

  spi_ioc_transfer transfer = {
    .tx_buf = (uint64_t)tx_buf,
    .rx_buf = (uint64_t)rx_buf
  };

  // Send header
  memcpy(tx_buf, &header, sizeof(header));
  add_checksum(tx_buf, sizeof(header));
  transfer.len = sizeof(header) + 1;
  ret = util::safe_ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
  if (ret < 0) {
    LOGE("SPI: failed to send header");
    goto transfer_fail;
  }

  // Wait for (N)ACK
  tx_buf[0] = 0x12;
  transfer.len = 1;
  ret = wait_for_ack(transfer, SPI_HACK);
  if (ret < 0) {
    goto transfer_fail;
  }

  // Send data
  if (tx_data != NULL) {
    memcpy(tx_buf, tx_data, tx_len);
  }
  add_checksum(tx_buf, tx_len);
  transfer.len = tx_len + 1;
  ret = util::safe_ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
  if (ret < 0) {
    LOGE("SPI: failed to send data");
    goto transfer_fail;
  }

  // Wait for (N)ACK
  tx_buf[0] = 0xab;
  transfer.len = 1;
  ret = wait_for_ack(transfer, SPI_DACK);
  if (ret < 0) {
    goto transfer_fail;
  }

  // Read data len
  transfer.len = 2;
  transfer.rx_buf = (uint64_t)(rx_buf + 1);
  ret = util::safe_ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
  if (ret < 0) {
    LOGE("SPI: failed to read rx data len");
    goto transfer_fail;
  }
  rx_data_len = *(uint16_t *)(rx_buf+1);
  if (rx_data_len >= SPI_BUF_SIZE) {
    LOGE("SPI: RX data len larger than buf size %d", rx_data_len);
    goto transfer_fail;
  }

  // Read data
  transfer.len = rx_data_len + 1;
  transfer.rx_buf = (uint64_t)(rx_buf + 2 + 1);
  ret = util::safe_ioctl(spi_fd, SPI_IOC_MESSAGE(1), &transfer);
  if (ret < 0) {
    LOGE("SPI: failed to read rx data");
    goto transfer_fail;
  }
  if (!check_checksum(rx_buf, rx_data_len + 4)) {
    LOGE("SPI: bad checksum");
    goto transfer_fail;
  }

  if (rx_data != NULL) {
    memcpy(rx_data, rx_buf + 3, rx_data_len);
  }

  return rx_data_len;

transfer_fail:
  return ret;
}
#endif
