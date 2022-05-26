# synergy-serial
Symless synergy client that sends mouse&keyboard input over UART/Arduino to another computer

This is a standalone client for [Synergy software](https://symless.com/synergy) which connects to the server, receives mouse&keyboard events, but instead of sending them to the system, it sends them over UART to another computer.

## Why?
This way the target computer doesn't require any custom software, not even a synergy client. Useful for business laptops with software restrictions.

In a regular synergy use-case everything looks like this:

```
+-------------------+           +------------------+
|        PC1        |    TCP    |        PC2       |
|                   | --------> |                  |
|  Synergy server   |           |  Synergy client  |
+-------------------+           +------------------+
     ^         ^
     |         |

      Physical
  Mouse & Ketboard
   (e.g. over USB)
```

With synergy-serial:
```
+------------------------------------------------------+        +--------------------+
|                          PC1                         |        |         PC2        |
|                                                      |        |                    |
|  +------------------+           +------------------+ |        |                    |
|  |                  |           |                  | |        |                    |
|  |  Synergy server  |    TCP    |  Synergy-serial  | |        |                    |
|  |                  | <-------> |      client      | |        |                    |
|  |                  |           |                  | |        |                    |
|  +------------------+           +---------+--------+ |        |                    |
|      ^          ^                         |          |        |                    |
+------+----------+-------------------------+----------+        +--------------------+
       |          |                         |                              ^
       |          |                         v                              | USB
       |          |                 +---------------+             +--------+-------+
       |          |                 |               |    UART     |                |
         Physical                   |  USB to TTL   +-----------> |     Arduino    |
      Mouse & Keyboard              |   converter   |             |    Pro Micro   |
       (e.g. over USB)              |               |             |                |
                                    +---------------+             +----------------+
```

Arduino Pro Micro was used because it has a standalone & fast hardware UART with RX/TX pins well exposed, but any other Arduino w/ USB should work fine too.

# Usage

Synergy-serial will connect to a synergy server at 127.0.0.1 and will work straight away, but you might want to modify the following options in `config.h`
```
#define CONFIG_HOSTNAME "red"
#define CONFIG_SCREENW (1920 * 2)
#define CONFIG_SCREENH 1080
```

```
make && ./build/synergy-serial -d /dev/ttyUSB1 -b 115200
```
