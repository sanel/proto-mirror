/* Random point-to-point radio simulation

Copyright (C) 2009, Nelson Elhage and contributors listed in the
AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory.  */

#ifndef __WORMHOLERADIO__
#define __WORMHOLERADIO__

#include "spatialcomputer.h"
#include "radio.h"
#include <set>

class WormHoleRadioDevice;

class WormHoleRadio : public RadioSim {
public:
  int n_wormholes;
  int devices_left;

  WormHoleRadio(Args *args, SpatialComputer *parent, int n);
  ~WormHoleRadio();

  BOOL handle_key(KeyEvent* key);
  void add_device(Device* d);

  NUM_VAL read_radio_range (VOID);
  int radio_send_export (uint8_t version, uint8_t timeout, uint8_t n, 
                         uint8_t len, COM_DATA *buf);
  int radio_send_script_pkt (uint8_t version, uint16_t n, 
                             uint8_t pkt_num, uint8_t *script);
  int radio_send_digest (uint8_t version, uint16_t script_len, 
                         uint8_t *digest);

private:
  WormHoleRadioDevice *random_device();
  void attach_wormholes();
  void connect_devices(WormHoleRadioDevice *, WormHoleRadioDevice *);
};

class WormHoleRadioDevice : public DeviceLayer {
  WormHoleRadio *parent;

  std::set<WormHoleRadioDevice*> nbrs;

  friend class WormHoleRadio;

public:
  WormHoleRadioDevice(WormHoleRadio *parent, Device *container);
  ~WormHoleRadioDevice();

  void visualize();

  void copy_state(DeviceLayer* src) {}
};

#endif