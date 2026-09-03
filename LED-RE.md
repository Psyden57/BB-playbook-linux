# LED-FAN5702.so reverse engineering — PlayBook RGB indicator LED

Target: `device-binaries/led-fan5702.so` (18,320 B, ARM32 LE, QNX 6.6 DLL, symbols stripped but **sections intact** — plain `arm-unknown-nto-qnx6.6.0eabi-objdump -d` works, no raw-segment extraction needed).
All code addresses below are file/VMA offsets inside the .so (LOAD base 0). PLT stubs were resolved via `.rel.plt` (GOT anchor = stub_pc + 0x4000; e.g. stub `0xc54` → GOT `0x504c` = `devctl`).

Hardware (cross-verified, not guessed): **Fairchild/onsemi FAN5702** "Configurable 180 mA 6-LED Driver with I²C Control" (datasheet FAN5702/D). Datasheet facts used below: slave address, register map `GENERAL@0x10 / CONFIG@0x20 / CHA@0xA0 / CH3@0x30 / CH4@0x40 / CH5@0x50 / CH6@0x60`, write = `S addr(W) A reg A data A P`, read = `S addr(W) A reg A Sr addr(R) A data A P` (repeated START), EN pin low = chip reset to default registers.

---

## 1. How the driver is started

Not in the IFS `.script`. It is a DLL loaded by the **winchester-lc "Light Control"** daemon, started from `/base/scripts/startup.sh` (`SERVICE_lc`, lines 1016–1045 of the dump in this workspace):

```sh
case "${uname_m}" in
    *Rev:00*|*Rev:01*|*Rev:02*) winchester-lc -d led-gpio.so    -d ls-cm3217.so ;;
    *)                          winchester-lc -d led-fan5702.so -d ls-cm3217.so ;;
esac
WAITFOR "lc" /pps/services/led/status
led_pps_server
```

* Board `Rev:07` (our unit) → `winchester-lc -d led-fan5702.so -d ls-cm3217.so`. `ls-cm3217.so` is the ambient-light sensor plugin (irrelevant here).
* DLL entry point is the exported data symbol **`led_drv_entry` @ `0x50d0` (14 bytes in .data)**: `{0x14, init=0x1408, fini=0x1374, probe=0xe5c, 0}` — i.e. the framework calls `probe` (`fan5702_probe` @ `0xe5c`) then `init` (`fan5702_init` @ `0x1408`); `0x1374` = `fan5702_fini`.
* Exported callback `fan5702_read_pps_data` @ `0x1860` is a tail-call to `led_read_pps_data` @ `0x21e0` — invoked by the framework when the PPS object changes.
* No command-line args reach the DLL itself; verbose level comes in via the framework handle (`arg+12` → `dev->verbose`, stored at `0x1414` region of init).

## 2. I2C bus and slave address

* **Bus device path: `/dev/i2c3`** — built in `fan5702_probe` at `0x10a4–0x10b8`: `snprintf(buf,11,"%s%d","/dev/i2c",3)` (fmt `"%s%d"` @ `0x2920`, `"/dev/i2c"` @ `0x2928`, the `%d` = **3** from `mov r1,#3` at `0x1054`, also stored at `dev+56`). `open()` at `0x1110` (PLT `0xd2c`).
* `.script` line for that bus: `i2c-omap35xx-omap4 -p 0x48350000 -i 94 -l 420 -f --u 3` → **controller = OMAP4430 I2C4, base `0x48350000`, IRQ 94** (TRM Table 23-30: I2C4 = `0x4835 0000`, 256-byte register file).
* **Bus speed: 100 kHz** — `dev->speed = 0x186a0 = 100000` stored at `0x1074` (`dev+68`), applied via devctl `0x80040502` (`DCMD_I2C_SET_BUS_SPEED`) at `0x117c`.
* **Slave address: `0x36` (7-bit)** — byte constant `0x36` (`mov r0,#54`) stored at `0x1068` (`dev+64`), passed as u32 in every devctl send/recv buffer.
  * QNX i2c devctl takes the 7-bit address unshifted (the omap driver writes it straight into `I2C_SA`; corroborated: wdtkick uses slave `0x48` = TWL6030's 7-bit address over the same devctl API).
  * Datasheet confirms: FAN5702 slave address = **6Ch** 8-bit write byte = `0110110` + R/W → **7-bit `0x36`** (on-wire bytes `0x6C` write / `0x6D` read). The prompt's guess `0x63/0xC6` is wrong.

## 3. Register-level protocol

### QNX devctl wrappers

* **`led_write_reg(dev, reg, val)` @ `0x1978`** — devctl `0x80100505` (`DCMD_I2C_SEND`), n=18, buffer at `0x50fc` (header in .data, `0x5100`=2, `0x5104`=2, `0x5108`=1):
  `{slave=0x36 (u32), len=2, stop=2, restart=1}` + inline data `{reg, val}` → a plain 2-byte I2C write: `[reg, val]`.
* **`led_read_reg(dev, reg, *val)` @ `0x186c`** — devctl `0xc0140507` (`DCMD_I2C_RECV`, read-direction flag `0xc014`), n=42, buffer at `0x50e4`: `{slave=0x36, len=2, stop=1, restart=1, 0}` with `reg` at `buf+20` (`0x50f8`); the received value is read back from the **same slot** (`0x1914–0x1920`). Semantics (matches driver's own log `"i2c_devaddr = %02x, reg_addr = %02x, reg_val = %02x"` and datasheet Fig. 24): write `reg`, repeated START, read value.
* **`led_update_reg(dev, reg, mask, op)` @ `0x1a7c`** — read-modify-write: `led_read_reg`, apply op to the byte, `led_write_reg`. Ops (switch at `0x1ad4`): **0=SET (val|mask), 1=AND, 2=XOR, 3=CLEAR (val&~mask)**; other → EINVAL(22).

### Registers the driver actually touches

Only **one register**: `GENERAL` @ **`0x10`** (FAN5702 datasheet Table 4).

* **Init** (`fan5702_probe`, devctl at `0x1208`, buffer `{0x36, 2, 2, 1, 0x10, 0x00}` on stack): **write `0x10 ← 0x00`** = all LED channels off, EN pin = enable function, full-scale 20 mA.
* **Colors** (`pps_led_parse` @ `0x1bb8`), all on `GENERAL (0x10)`, via `led_update_reg`:

  | Color | GENERAL bit | mask | set/call site |
  |---|---|---|---|
  | RED   | bit1 (EN3) | `0x02` | set `0x1e84–0x1e90` (op 0), clear `0x1f54–0x1f60` (op 3) |
  | GREEN | bit2 (EN4) | `0x04` | set `0x1ff4–0x2000` (op 0), clear `0x20c8–0x20d4` (op 3) |
  | BLUE  | bit3 (EN5) | `0x08` | set `0x1d10–0x1d1c` (op 0), clear `0x1de0–0x1dec` (op 3) |

  Per datasheet, `GENERAL` = `{7 PWM, 6:5 FS[1:0], 4 EN6, 3 EN5, 2 EN4, 1 EN3, 0 ENA}`. So R/G/B = **FAN5702 output channels D3/D4/D5** (EN3/EN4/EN5). (Which physical FAN5702 pin each color sits on is board wiring — inferred, marked as such — but the QNX naming "red/green/blue" for bits 1/2/3 is RIM's own and definitive for the PlayBook.)
* **Brightness**: the QNX driver **never writes** the per-channel dimming registers; channels run at power-up default = dimming code `111111` = 100 % of full scale (20 mA, FS bits `00`). Available for bare-metal use (datasheet-derived, unverified on device): `CH3@0x30` (R), `CH4@0x40` (G), `CH5@0x50` (B), write value `0b11xxxxxx` (bits 7:6 must stay 1) with 6-bit dimming code `0–63` = 0–100 %.
* **Full init sequence** (`fan5702_probe`, `0xe5c`):
  1. `ThreadCtl(_NTO_TCTL_IO)` (`0xe94`).
  2. `mmap_device_io(0x1000, 0x4A310000)` (`0xee8–0xef4`) — **GPIO1** (TRM: L4_WKUP, GPIO1 module @ `0x4A31 0000`; this also resolves the PLAYBOOK-REFERENCE §1.8 mystery "winchester-lc maps 0x4A310000 = odd WDT1 region": it is GPIO1, the LED **EN pin**).
  3. `*0x4A310190 = 0x00002000` (`0xf38`) — GPIO1 `CLEARDATAOUT` bit13 → EN low (chip in reset).
  4. `*0x4A310134 &= ~0x00002000` (read-modify-write, IRQs masked around it, `0xf6c–0xf80`) — GPIO1 `GPIO_OE` bit13 → **output**.
  5. `clock_nanosleep({0,100ns})` (`0xfac–0xfbc`).
  6. `*0x4A310194 = 0x00002000` (`0xff4–0x1008`) — GPIO1 `SETDATAOUT` bit13 → **EN high** (chip enabled, registers at default).
  7. `clock_nanosleep({0,100ns})`.
  8. `open("/dev/i2c3")`, devctl SET_BUS_SPEED=100000.
  9. I2C write `[0x10, 0x00]` (GENERAL = all off).

  So **FAN5702 EN = GPIO1_13** (`0x4A310000` bit 13; TRM offsets: `GPIO_OE=0x134`, `GPIO_CLEARDATAOUT=0x190`, `GPIO_SETDATAOUT=0x194`). EN low resets the chip and forces all registers/LEDs to default(off) — a hard "LED off" switch independent of I2C.

## 4. PPS interface → colors

* Check `access("/pps/")` (`0x1450`, string `0x2b2c`); build path `snprintf("%s%s%s", "/pps/", "services/led/", "status")` (`0x14a8–0x14c8`, strings `0x2b2c`/`0x2b70`/`0x2b80`) → **`/pps/services/led/status`**.
* If the object already exists it is `unlink`ed ("LED PPS object already exists, removing", `0x14ac–0x155c`).
* `pps_create_led_objects` @ `0x234c` → `pps_create_object(fd, "/pps/services/led/", "status", ...)`.
* Initial content written: `blue::off`, `green::off`, `red::off` (strings `0x2c70/0x2c7c/0x2c88`), logged as `"write %d char in %s%s%s"`.
* `led_read_pps_data` @ `0x21e0`: `read(ppsfd, buf, 1023)` → `pps_led_parse(buf)` → `ionotify(fd, POLLARM, ...)` to wait for the next change (re-armed each cycle).
* `pps_led_parse` @ `0x1bb8`: line loop; leading `@status` header line is recognized (strcmp @ `0x1ca0`, logged "PPS project name") and skipped; other lines matched **exactly** (strcmp) against six strings and each triggers `led_update_reg(dev, 0x10, mask, op)`:

  | PPS line | action |
  |---|---|
  | `blue::on` / `blue::off` | `0x10` \|/\&~ `0x08` |
  | `red::on` / `red::off` | `0x10` \|/\&~ `0x02` |
  | `green::on` / `green::off` | `0x10` \|/\&~ `0x04` |
  | anything else | logged "invalid PPS attribute" and ignored |

  Only on/off exists — no brightness or pattern attributes. Patterns (blinking etc.) are produced by the framework/timers repeatedly writing these lines. State tracking: `dev+36` → ledstatus shadow word, per-line counters, and `led_update_reg`'s read-modify-write makes individual attribute updates safe.

### Testing from QNX userspace (root SSH)

```sh
echo red::on > /pps/services/led/status          # red
printf 'green::on\n' > /pps/services/led/status  # + green = yellow
printf 'blue::on\n'  > /pps/services/led/status  # + blue = white
printf 'red::off\ngreen::off\nblue::off\n' > /pps/services/led/status   # off
# blink blue:
while :; do echo blue::on > /pps/services/led/status; sleep 1; \
           echo blue::off > /pps/services/led/status; sleep 1; done
sloginfo | grep '^LED'    # driver diagnostics ("turn on RED_LED", reg dumps on error)
```

## 5. Minimal bare-metal recipe (MMU off, no OS)

State at kexec-jump time: QNX has I2C4 and GPIO1 clocked, I2C4 configured for 100 kHz, EN=high, GENERAL=off. Simplest payload: **do not re-init anything**, just drive I2C4 registers. (If you do re-init later, see §6.)

OMAP4 HS-I2C register offsets from base `0x48350000` (TRM Table 23-31/32; **16-bit accesses only — 32-bit access corrupts registers**): `SYSC 0x10, IRQSTATUS_RAW 0x24, IRQSTATUS 0x28, IRQENABLE_SET 0x2C, IRQENABLE_CLR 0x30, STAT 0x88, SYSS 0x90, BUF 0x94, CNT 0x98, DATA 0x9C, CON 0xA4, OA 0xA8, SA 0xAC, PSC 0xB0, SCLL 0xB4, SCLH 0xB8`.
`I2C_CON` bits: [15] I2C_EN, [10] MST, [9] TRX, [2] RM, [1] STP, [0] STT (TRM Fig. 23-29: master TX polling = `CON = 0x8603`); `I2C_STAT/IRQSTATUS`: [0] AL, [1] NACK, [2] ARDY, [3] RRDY, [4] XRDY, [12] BB. All status bits are write-1-to-clear.

**One register write = `[reg, val]`, e.g. solid RED:**

```c
#define I2C4 0x48350000
#define GPIO1 0x4A310000
u16 REG(off) = *(volatile u16*)(I2C4+off);           /* 16-bit only! */

/* (a) make sure the chip is out of reset: GPIO1_13 = EN = high      */
*(volatile u32*)(GPIO1+0x194) = 0x00002000;          /* SETDATAOUT    */

/* (b) write GENERAL (0x10) = 0x02  -> RED on, others off            */
while (REG(0x88) & (1<<12)) ;                        /* wait !BB      */
REG(0x98) = 2;                                       /* CNT  = 2      */
REG(0xAC) = 0x36;                                    /* SA   = 0x36   */
REG(0x9C) = 0x10;                                    /* DATA = reg    */
REG(0xA4) = 0x8603;                                  /* EN|MST|TRX|STP|STT */
while (!(REG(0x88) & (1<<4))) ;                      /* XRDY          */
REG(0x9C) = 0x02;                                    /* DATA = val    */
while (!(REG(0x88) & (1<<2))) ;                      /* ARDY          */
if (REG(0x88) & 3) { REG(0x28) = REG(0x88); /* NACK/AL -> handle */ }
REG(0x28) = REG(0x88);                               /* clear status  */
```

Color byte for `GENERAL @0x10` (bits 5–7 must stay 0: 7=PWM-mode, 6:5=full-scale):

| effect | value |
|---|---|
| off | `0x00` |
| red | `0x02` |
| green | `0x04` |
| blue | `0x08` |
| yellow | `0x06` |
| cyan | `0x0C` |
| magenta | `0x0A` |
| white | `0x0E` |

Read-back (e.g. verify GENERAL): phase 1 `CNT=1, DATA=reg, CON=0x8601` (STT, no STP) → wait XRDY/ARDY; phase 2 `CNT=1, CON=0x8403` (MST, RX, STP+STT → repeated START) → wait RRDY (stat bit3) → read `DATA` (TRM multi-phase flow, (STT;STP)=(1;0)…(0;1)).

Blinking: loop `write 0x10 = mask` / `= 0`, paced by the 32-kHz timer or a busy loop; no interrupts needed. Per-channel brightness (datasheet-derived, optional): write `[0x30|0x40|0x50, 0xC0|code]`, code 0–63, channels R/G/B respectively.
Hard kill switch: `*(volatile u32*)(GPIO1+0x190) = 0x2000` (EN low → chip reset, all LEDs off).

## 6. Clock / gating requirements

* **I2C4**: base `0x48350000` (TRM Table 23-30). Gate: **`CM_L4PER_I2C4_CLKCTRL` @ `0x4A0094B8`** (CM2; set `MODULEMODE[1:0]=2` = ENABLE, poll `IDLEST[17:16]=0`). Functional/interface clocks come from the APE PRCM (TRM §23.1.2.1.3.11). 100 kHz is QNX's configured speed (via `PSC/SCLL/SCLH @0xB0/0xB4/0xB8`); already set before the jump — do not touch unless re-initializing from scratch (then: software reset via `SYSC@0x10`, wait `SYSS@0x90` RDONE, program PSC/SCLL/SCLH from the module functional clock, TRM §23.1.6 programming sequence).
* **GPIO1** (EN pin): L4_WKUP domain, gate **`CM_WKUP_GPIO1_CLKCTRL` @ `0x4A307838`** (`MODULEMODE=2`). WKUP-domain GPIO is in the always-on power/clock domain — it keeps working with MMU off regardless of core state. Padconf: the EN pad is already muxed to GPIO by QNX's boot; only `GPIO_OE`/`SETDATAOUT`/`CLEARDATAOUT` are needed.
* **FAN5702 itself**: charge-pump, self-managed (1x/1.5x automatic), startup time `tSTART` ≈ 250 µs from EN high to VOUT ready (datasheet) — after forcing EN low→high, delay ≥1 ms before the first I2C byte (QNX only sleeps 100 ns but its open/devctl path provides the real delay).

## Source map (quick reference)

| symbol | addr | notes |
|---|---|---|
| `fan5702_probe` | `0x0e5c` | GPIO1 EN poke, `/dev/i2c3` open, 100 kHz, `0x10←0x00` |
| `fan5702_fini` | `0x1374` | close fds, `munmap_device_io` |
| `fan5702_init` | `0x1408` | `/pps/services/led/status` create + initial off-state |
| `fan5702_read_pps_data` | `0x1860` | tail-call → `led_read_pps_data` |
| `led_read_reg` | `0x186c` | devctl `0xc0140507` RECV, reg/value @ buf+20 (`0x50f8`) |
| `led_write_reg` | `0x1978` | devctl `0x80100505` SEND, `[reg,val]` @ `0x510c` |
| `led_update_reg` | `0x1a7c` | RMW; ops 0=OR 1=AND 2=XOR 3=BIC |
| `pps_led_parse` (static) | `0x1bb8` | line matcher, R=0x02 G=0x04 B=0x08 on reg `0x10` |
| `led_read_pps_data` | `0x21e0` | read 1023 B + parse + ionotify |
| `pps_create_led_objects` | `0x234c` | pps_create_object("/pps/services/led/", "status") |
| `led_drv_entry` (.data) | `0x50d0` | `{0x14, 0x1408, 0x1374, 0xe5c, 0}` |
| devctl cmd buffers (.data) | `0x50e4` (recv) / `0x50fc` (send) | slave `0x36`, len/stop/restart headers |
| rodata strings | `0x2880–0x32a0` | all "LED: ..." logs, `/dev/i2c`, `/pps/...`, color lines |
