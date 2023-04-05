#define TMSPIN 2
#define TCKPIN 3
#define TDIPIN 4
#define TDOPIN 7
#define TNRSTPIN 8
#define TNRDYPIN 12

#define BLOCKSIZE 16
#define CMD_PING 'P'
#define RES_GD 'G'
#define RES_NG 'N'
#define RES_RP 'R'

uint16_t tck_delay_cycles = 0;
void TCKSet(uint8_t value)
{
  for (uint16_t i = tck_delay_cycles; i--;);
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
  pinMode(TNRDYPIN, INPUT);
  Serial.begin(115200);
  JTAGReset();
}

char buffer[BLOCKSIZE + 1];
uint8_t serptr = 0;
bool write = false;

uint8_t serial_chksum()
{
  uint8_t chksum;
  for (uint8_t i = 0; i < BLOCKSIZE; i++) {
    chksum += buffer[i];
  }
  return chksum;
}

void execute_serial_cmd()
{
  uint8_t chksum = serial_chksum();
  if ((uint8_t)(chksum + buffer[BLOCKSIZE])) {
    buffer[0] = RES_RP;
    buffer[BLOCKSIZE] = -serial_chksum();
    return;
  }

  switch (buffer[0]) {
    case CMD_PING:
      buffer[0] = RES_GD;
      break;
    default:
      buffer[0] = RES_NG;
      buffer[1] = 0;
      break;
  }

  buffer[BLOCKSIZE] = -serial_chksum();
}

void loop()
{
  if (serptr <= BLOCKSIZE) {
    if (!write) {
      serptr += Serial.readBytes(buffer + serptr, BLOCKSIZE + 1 - serptr);
    }
    else {
      int avail = Serial.availableForWrite();
      if (avail > BLOCKSIZE)
          avail = BLOCKSIZE + 1;
      serptr += Serial.write(buffer + serptr, BLOCKSIZE + 1 - serptr);
    }
  }
  else {
    if (!write) {
      execute_serial_cmd();
    }
    write = !write;
    serptr = 0;
  }
}
