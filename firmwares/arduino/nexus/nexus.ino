#define TMSPIN 2
#define TCKPIN 3
#define TDIPIN 4
#define TDOPIN 7
#define TNRSTPIN 8
#define TNRDYPIN 12

#define BLOCKSIZE 16
#define CMD_PING 'P'
#define CMD_RESET 'T'
#define CMD_JR_RW 'W'
#define CMD_CONFIG 'C'
#define RES_GD 'G'
#define RES_NG 'N'
#define RES_RP 'R'

#define CAPTURE_IR_FLAG 0x80
#define WAIT_READY_FLAG 0x40
#define TO_IDLE_FLAG 0x20

#define JTAG_STATE_IDLE 0
#define JTAG_STATE_SELECT_DR 1

#define JTAG_REG_MAX_LEN 96

#define ERR_NOTCMD 0xff
#define ERR_BADREGLEN 0xfe

static uint8_t g_buffer[BLOCKSIZE + 1];
static uint8_t g_serptr = 0;
static uint8_t g_jtag_state = JTAG_STATE_IDLE;
static bool g_write_state = false;
static uint16_t g_tck_delay_cycles = 0;

void TCKSet(uint8_t value)
{
  for (volatile uint16_t i = g_tck_delay_cycles; i; i--);
  digitalWrite(TCKPIN, value);
}

// Use both TNRST and TMS to doubly make
// sure that the JTAG controller is really reset.
void JTAGReset()
{
  digitalWrite(TNRSTPIN, 0);
  delay(1);
  digitalWrite(TNRSTPIN, 1);
  digitalWrite(TMSPIN, 1);
  for (uint8_t i = 0; i < 5; i++) {
    TCKSet(0);
    TCKSet(1);
  }
  digitalWrite(TMSPIN, 0);
  TCKSet(0);
  TCKSet(1);
}

void setup()
{
  pinMode(TNRSTPIN, OUTPUT);
  pinMode(TCKPIN, OUTPUT);
  pinMode(TMSPIN, OUTPUT);
  pinMode(TDIPIN, OUTPUT);
  pinMode(TDOPIN, INPUT);
  pinMode(TNRDYPIN, INPUT_PULLUP);
  Serial.begin(115200);
  JTAGReset();
}

uint8_t serial_chksum()
{
  uint8_t chksum;
  for (uint8_t i = 0; i < BLOCKSIZE; i++) {
    chksum += g_buffer[i];
  }
  return chksum;
}

static void set_proto_err(uint8_t err)
{
  g_buffer[0] = RES_NG;
  g_buffer[1] = err;
}

void execute_serial_cmd()
{
  uint8_t chksum = serial_chksum();
  uint8_t cmd = g_buffer[0];
  if ((uint8_t)(chksum + g_buffer[BLOCKSIZE])) {
    g_buffer[0] = RES_RP;
    g_buffer[BLOCKSIZE] = -serial_chksum();
    return;
  }

  g_buffer[0] = RES_GD;
  switch (cmd) {
    case CMD_PING:
      break;
    case CMD_RESET:
      JTAGReset();
      break;
    case CMD_CONFIG:
      break;
    case CMD_JR_RW: {
      uint8_t datalen = g_buffer[1];
      uint8_t flags = g_buffer[2];

      if (datalen > JTAG_REG_MAX_LEN) {
        set_proto_err(ERR_BADREGLEN);
        break;
      }

      // IDLE -> Select DR
      digitalWrite(TMSPIN, 1);
      if (g_jtag_state == JTAG_STATE_IDLE) {
        TCKSet(0);
        TCKSet(1);
        g_jtag_state = JTAG_STATE_SELECT_DR;
      }
      // Select DR -> Select IR
      if (flags & CAPTURE_IR_FLAG) {
        TCKSet(0);
        TCKSet(1);
      }
      // Select -> Capture
      digitalWrite(TMSPIN, 0);
      TCKSet(0);
      TCKSet(1);
      // Capture -> Shift
      TCKSet(0);
      TCKSet(1);
      // Do the shift
      for (uint8_t i = 0; i < JTAG_REG_MAX_LEN; i++) {
        uint8_t bit = g_buffer[4 + (i >> 3)] >> (i & 7) & 1;
        TCKSet(0);
        g_buffer[4 + (i >> 3)] |= digitalRead(TDOPIN) << (i & 7);
        digitalWrite(TDIPIN, bit);
        TCKSet(1);
      }
      // Shift -> Exit1
      digitalWrite(TMSPIN, 1);
      TCKSet(0);
      TCKSet(1);
      // Exit1 -> Update
      TCKSet(0);
      TCKSet(1);
      // Update -> Idle or Select DR
      // This may or may not be necessary for NEXUS-ACCESS?
      // Examples give transitions immediately to SELECT DR.
      if (flags & TO_IDLE_FLAG) {
        digitalWrite(TMSPIN, 0);
        g_jtag_state = JTAG_STATE_IDLE;
      }
      TCKSet(0);
      TCKSet(1);
      break;
    }
    default:
      set_proto_err(ERR_NOTCMD);
      break;
  }

  g_buffer[BLOCKSIZE] = -serial_chksum();
}

void loop()
{
  if (g_serptr <= BLOCKSIZE) {
    if (!g_write_state) {
      g_serptr += Serial.readBytes(g_buffer + g_serptr, BLOCKSIZE + 1 - g_serptr);
    }
    else {
      int avail = Serial.availableForWrite();
      if (avail > BLOCKSIZE)
          avail = BLOCKSIZE + 1;
      g_serptr += Serial.write(g_buffer + g_serptr, BLOCKSIZE + 1 - g_serptr);
    }
  }
  else {
    if (!g_write_state) {
      execute_serial_cmd();
    }
    g_write_state = !g_write_state;
    g_serptr = 0;
  }
}
