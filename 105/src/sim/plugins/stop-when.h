/* Programmed simulation termination

Copyright (C) 2009, Nelson Elhage and contributors listed in the
AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory.  */

#ifndef  __STOPWHEN__
#define  __STOPWHEN__

#include "spatialcomputer.h"
#include <set>

class StopWhen : public Layer, public HardwarePatch {
  public:
  float stop_pct;

  StopWhen(Args *args, SpatialComputer *parent);
  ~StopWhen();

  void add_device(Device *d);

  void set_probe (DATA* d, uint8_t p);

 private:
  int n_devices;
  std::set<Device*> probed;
};

#endif
