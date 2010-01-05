/* Top-level spatial computer classes
Copyright (C) 2005-2008, Jonathan Bachrach, Jacob Beal, and contributors 
listed in the AUTHORS file in the MIT Proto distribution's top directory.

This file is part of MIT Proto, and is distributed under the terms of
the GNU General Public License, with a linking exception, as described
in the file LICENSE in the MIT Proto distribution's top directory. */

#include "config.h"
#include "spatialcomputer.h"
#include "visualizer.h"

/*****************************************************************************
 *  MISC DEFS                                                                *
 *****************************************************************************/

Layer::Layer(SpatialComputer* p) { parent=p; can_dump=p->is_dump_default; }

/*****************************************************************************
 *  DEVICE                                                                   *
 *****************************************************************************/
int Device::top_uid=0; // originally, the device UIDs start at zero

Device::Device(SpatialComputer* parent, METERS *loc, DeviceTimer *timer) { 
  uid=top_uid++; this->timer = timer; this->parent = parent;
  run_time=0;  // should be reset at script-load
  body = parent->physics->new_body(this,loc[0],loc[1],loc[2]);
  // integrate w. layers, which may add devicelayers to the device
  num_layers = parent->dynamics.max_id();
  layers = (DeviceLayer**)calloc(num_layers,sizeof(DeviceLayer*));
  for(int i=0;i<num_layers;i++) 
    { Layer* l = (Layer*)parent->dynamics.get(i); if(l) l->add_device(this); }
  vm = allocate_machine(); // unusable until script is loaded
  is_selected=FALSE; is_debug=FALSE;
}

// copy all state
Device* Device::clone_device(METERS *loc) {
  Device* new_d = new Device(parent,loc,timer->clone_device());

  //void Device::load_script(uint8_t* script, int len) {
  //  new_machine(vm, uid, 0, 0, 0, 1, script, len);
  uint8_t *copy_src = vm->membuf, *copy_dst = new_d->vm->membuf;
  for(int i=0;i<vm->memlen;i++) { copy_dst[i]=copy_src[i]; }
  new_d->load_script(vm->script,vm->scripts[vm->cur_script].len);
  // copy over state
  
  new_d->vm->time = vm->time;  new_d->vm->last_time = vm->last_time;
  new_d->run_time=run_time; new_d->is_selected=is_selected; 
  new_d->is_debug=is_debug;
  for(int i=0;i<num_layers;i++) {
    DeviceLayer *d = layers[i], *nd = new_d->layers[i]; 
    if(d && nd) nd->copy_state(d);
  }
  return new_d;
}

Device::~Device() {
  delete timer;
  delete body;
  for(int i=0;i<num_layers;i++)
    { DeviceLayer* d = (DeviceLayer*)layers[i]; if(d) delete d; }
  free(layers);
  deallocate_machine(&vm);
}

// dump function should produce matlab-readable data at verbosity 0
// and at verbosity 1+ should make human-readable
void Device::dump_state(FILE* out, int verbosity) {
  // dump heading information
  if(verbosity==0) {
    fprintf(out,"%d %.2f %.2f",uid,vm->ticks,vm->time);
  } else {
    fprintf(out,"Device %d ",uid);
    if(verbosity>=2) fprintf(out,"[in slot %d]",backptr);
    fprintf(out,"(Internal time: %.2f %.2f)\n",vm->ticks,vm->time);
    if(verbosity>=2) 
      fprintf(out,"Selected = %s, Debug = %s\n",bool2str(is_selected),
              bool2str(is_debug));
  }
  // dump layers
  if(parent->physics->can_dump) body->dump_state(out,verbosity);
  for(int i=0;i<num_layers;i++) {
    DeviceLayer* d = (DeviceLayer*)layers[i]; 
    Layer* l = (Layer*)parent->dynamics.get(i);
    if(d && l->can_dump) d->dump_state(out,verbosity);
  }
  // dump output
  if(parent->is_dump_value) {
    char buf[1000];
    if(verbosity==0) {
      post_stripped_data_to(buf, &vm->res);
      fprintf(out," %s",buf);
    } else {
      post_data_to(buf, &vm->res);
      fprintf(out,"Output %s\n",buf);
    }
  }
  
  // dump neighborhood data
  if(verbosity>=1 && parent->is_dump_hood) {
    char buf[1000];
    fprintf(out,"Export Values: "); // first export values
    for(int i=0;i<vm->n_hood_vals;i++) {
      post_data_to(buf,&vm->hood_exports[i]); fprintf(out,"%s ",buf);
    }
    fprintf(out,"\nNeighbor Values:\n"); // then neighbor data
    for(int i=0;i<vm->n_hood;i++) {
      NBR* nbr = vm->hood[i];
      fprintf(out,
              "Neighbor %4d [X=%.2f, Y=%.2f, Z=%.2f, Range=%.2f, Time=%.2f]: ",
              nbr->id, nbr->x, nbr->y, nbr->z,
              sqrt(nbr->x*nbr->x + nbr->y*nbr->y + nbr->z*nbr->z),
              nbr->stamp);
      if(nbr->stamp==-1) { 
	fprintf(out,"[INVALID]");
      } else {
	for(int j=0;j<vm->n_hood_vals;j++) {
	  post_data_to(buf,&nbr->imports[j]); fprintf(out,"%s ",buf);
	}
      }
      fprintf(out,"\n");
    }
  }
  if(verbosity==0) fprintf(out,"\n"); // terminate line
}

void Device::load_script(uint8_t* script, int len) {
  new_machine(vm, uid, 0, 0, 0, 1, script, len);
}

// a convenient combined function
BOOL Device::debug() { return is_debug && parent->is_debug; }

// scale the display to draw text labels for a device
#define TEXT_SCALE 3.75 // arbitrary constant for text sizing
void Device::text_scale() {
#ifdef WANT_GLUT
  flo d = body->display_radius(); glScalef(d,d,d); // scale text to body
  d = vis_context->display_mag; glScalef(d,d,d); // then magnify as specified
  glScalef(TEXT_SCALE,TEXT_SCALE,TEXT_SCALE);
#endif // WANT_GLUT
}

void Device::internal_event(SECONDS time, DeviceEvent type) {
  switch(type) {
  case COMPUTE:
    body->preupdate(); // run the pre-compute update
    for(int i=0;i<num_layers;i++)
      { DeviceLayer* d = (DeviceLayer*)layers[i]; if(d) d->preupdate(); }
    exec_machine(vm->ticks+1, time); // do the actual computation
    body->update(); // run the post-compute update
    for(int i=0;i<num_layers;i++)
      { DeviceLayer* d = (DeviceLayer*)layers[i]; if(d) d->update(); }
    break;
  case BROADCAST:
    export_machine();
    if(script_export_needed() || (((int)vm->ticks) % 10)==0) {
      export_script(); // send script every 10 rounds, or as needed
    }
    break;
  }
}

BOOL Device::handle_key(KeyEvent* key) {
  for(int i=0;i<num_layers;i++) {
    DeviceLayer* d = (DeviceLayer*)layers[i]; 
    if(d && d->handle_key(key)) return TRUE;
  }
  return FALSE;
}

void Device::visualize() {
#ifdef WANT_GLUT
  glPushMatrix();
  // center on device
  const flo* p = body->position(); glTranslatef(p[0],p[1],p[2]);
  // draw the body & other dynamics layers
  body->visualize();
  for(int i=0;i<num_layers;i++)
    { DeviceLayer* d = (DeviceLayer*)layers[i]; if(d) d->visualize(); }
  
  if(is_selected) {
    palette->use_color(DEVICE_SELECTED);
    draw_circle(4*body->display_radius());
  }
  if(debug()) {
    palette->use_color(DEVICE_DEBUG);
    glPushMatrix();
    glTranslatef(0,0,-0.1);
    draw_disk(2*body->display_radius());
    glPopMatrix();
  }

  if (vis_context->is_show_vec) {
    DATA *dst = &vm->res;
    glPushMatrix();
    glLineWidth(4);
    glTranslatef(0, 0, 0);
    switch (dst->tag) {
    case VEC_TAG: {
      VEC_VAL *v;
      v = VEC_GET(dst);
      if (v->n >= 2) {
        flo x = NUM_GET(&v->elts[0]);
        flo y = NUM_GET(&v->elts[1]);
        flo z = v->n >= 3 ? NUM_GET(&v->elts[2]) : 0;
	palette->use_color(VECTOR_BODY);
        glBegin(GL_LINE_STRIP);
        glVertex3f(0, 0, 0);
        glVertex3f(0.8*x, 0.8*y, 0.8*z);
        glEnd();
	palette->use_color(VECTOR_TIP);
        glBegin(GL_LINE_STRIP);
        glVertex3f(0.8*x, 0.8*y, 0.8*z);
        glVertex3f(x, y, z);
        glEnd();
      }
      break; }
    }
    glLineWidth(1);
    glPopMatrix();
  }
  
  text_scale(); // prepare to draw text
  char buf[1024];
  if (vis_context->is_show_id) {
    palette->use_color(DEVICE_ID);
    sprintf(buf, "%2d", uid);
    draw_text(1, 1, buf);
  }

  if(vis_context->is_show_val) {
    DATA *dst = &vm->res;
    glPushMatrix();
    //glTranslatef(0, 0, 0);
    post_data_to(buf, dst);
    palette->use_color(DEVICE_VALUE);
    draw_text(1, 1, buf);
    glPopMatrix();
  }
  
  if(vis_context->is_show_version) {
    glPushMatrix();
    palette->use_color(DEVICE_ID);
    sprintf(buf, "%2d:%s", vm->scripts[vm->cur_script].version,
	    (vm->scripts[vm->cur_script].is_complete)?"OK":"wait");
    draw_text(4, 4, buf);
    glPopMatrix();
  }
  glPopMatrix();
#endif // WANT_GLUT
}

// Special render for OpenGL selection mode
void Device::render_selection() {
#ifdef WANT_GLUT
  glPushMatrix();
  // center on device
  const flo* p = body->position(); glTranslatef(p[0],p[1],p[2]);
  glLoadName(backptr); // set the name
  body->render_selection(); // draw the body
  glPopMatrix();
#endif // WANT_GLUT
}  

/*****************************************************************************
 *  SIMULATED DEVICE                                                         *
 *****************************************************************************/
class SimulatedDevice : public Device {
public:
  SimulatedDevice(SpatialComputer* parent, METERS* loc, DeviceTimer *timer) :
    Device(parent,loc,timer) {}
  void dump_state(FILE* out, int verbosity) {
    if(verbosity>=1) fprintf(out,"(Simulated) ");
    Device::dump_state(out,verbosity);
  }
};



/*****************************************************************************
 *  SPATIAL COMPUTER                                                         *
 *****************************************************************************/
// get the spatial layout of the computer, including node distribution
void SpatialComputer::get_volume(Args* args, int n) {
  // spatial layout
  int dimensions=2; // default to 2D world
  METERS width=132, height=100, depth=0; // sides of the network distribution
  if(args->extract_switch("-3d")) { depth=40; dimensions=3; }
  if(args->extract_switch("-dim")) { // dim X [Y [Z]]
    width=args->pop_number();
    if(str_is_number(args->peek_next())) {
      height=args->pop_number();
      if(str_is_number(args->peek_next())) { 
        depth=args->pop_number(); dimensions=3;
      }
    }
  }
  if(dimensions==2)
    volume=new Rect(-width/2, width/2, -height/2, height/2);
  else
    volume=new Rect3(-width/2, width/2, -height/2, height/2, -depth/2, depth);
}

// add the layer to dynamics and set its ID, for use in hardware callbacks
int SpatialComputer::addLayer(Layer* layer) {
  return (layer->id = dynamics.add(layer));
}

SpatialComputer::SpatialComputer(Args* args) {
  sim_time=0;
  int n=(args->extract_switch("-n"))?(int)args->pop_number():100; // # devices
  // load dumping variables
  is_dump_default=TRUE;
  args->undefault(&is_dump_default,"-Dall","-NDall");
  is_dump_hood=is_dump_default;
  args->undefault(&is_dump_hood,"-Dhood","-NDhood");
  is_dump_value=is_dump_default;
  args->undefault(&is_dump_value,"-Dvalue","-NDvalue");
  is_dump = args->extract_switch("-D");
  is_probe_filter = args->extract_switch("-probe-dump-filter");
  is_show_snaps = !args->extract_switch("-no-dump-snaps");
  dump_start = args->extract_switch("-dump-after") ? args->pop_number() : 0;
  dump_period = args->extract_switch("-dump-period") ? args->pop_number() : 1;
  dump_dir = args->extract_switch("-dump-dir") ? args->pop_next() : "dumps";
  dump_stem = args->extract_switch("-dump-stem") ? args->pop_next() : "dump";
  just_dumped=FALSE; next_dump = dump_start; snap_vis_time=0;
  // setup customization
  get_volume(args, n);
  choose_layers(args,n);             // what types of physics apply

  scheduler = new Scheduler(n, time_model->cycle_time());
  // create the actual devices
  METERS loc[3];
  for(int i=0;i<n;i++) {
    if(distribution->next_location(loc)) {
      SECONDS start;
      Device* d = new SimulatedDevice(this,loc,time_model->next_timer(&start));
      d->backptr = devices.add(d);
      scheduler->schedule_event((void*)d->backptr,start,0,COMPUTE,d->uid);
    }
  }
  
  // load display variables
  display_mag = (args->extract_switch("-mag"))?args->pop_number():1;
  is_show_val = args->extract_switch("-v");
  is_show_vec = args->extract_switch("-sv");
  is_show_id = args->extract_switch("-i");
  is_show_version = args->extract_switch("-show-script-version");
  is_debug = args->extract_switch("-g");
  hardware.is_kernel_trace = args->extract_switch("-t");
  hardware.is_kernel_debug = args->extract_switch("-debug-kernel");
  hardware.is_kernel_debug_script = args->extract_switch("-debug-script");
}

SpatialComputer::~SpatialComputer() {
  // delete devices first, because their "death" needs dynamics to still exist
  for(int i=0;i<devices.max_id();i++)
    { Device* d = (Device*)devices.get(i); if(d) delete d; }
  // delete everything else in arbitrary order
  delete scheduler; delete volume; delete time_model; delete distribution;
  for(int i=0;i<dynamics.max_id();i++) 
    { Layer* ec = (Layer*)dynamics.get(i); if(ec) delete ec; }
}

// for the initial loading only
void SpatialComputer::load_script(uint8_t* script, int len) {
  for(int i=0;i<devices.max_id();i++) { 
    Device* d = (Device*)devices.get(i); 
    if(d) {
      hardware.set_vm_context(d);
      d->load_script(script,len); 
    }
  }
}
// install a script by injecting it as packets w. the next version
void SpatialComputer::load_script_at_selection(uint8_t* script, int len) {
  for(int i=0;i<selection.max_id();i++) {
    Device* d = (Device*)devices.get((long)selection.get(i));
    if(d) {
      hardware.set_vm_context(d);
      version++;
      for (i=0; i < (int)floor(len  / (float)MAX_SCRIPT_PKT); i++) {
	radio_receive_script_pkt(version, len, i, &script[i*MAX_SCRIPT_PKT]);
      }
      if (len % MAX_SCRIPT_PKT) {
	radio_receive_script_pkt(version, len, i, &script[i*MAX_SCRIPT_PKT]);
      }
    }
  }
}

BOOL SpatialComputer::handle_key(KeyEvent* key) {
  // is this a key recognized internally?
  if(key->normal && !key->ctrl) {
    switch(key->key) {
    case 'i': is_show_id = !is_show_id; return TRUE;
    case 'v': is_show_vec = !is_show_vec; return TRUE;
    case 'n': is_show_val = !is_show_val; return TRUE;
    case 'j': is_show_version = !is_show_version; return TRUE;
    case 'U': selection.clear(); update_selection(); return TRUE;
    case 'a': hardware.is_kernel_trace = !hardware.is_kernel_trace; return TRUE;
    case 'd': is_debug = !is_debug; return TRUE;
    case 'D':
      for(int i=0;i<selection.max_id();i++) {
        Device* d = (Device*)devices.get((long)selection.get(i));
        if(d) d->is_debug = !d->is_debug;
      }
      return TRUE;
    case '8': is_probe_filter=TRUE; is_dump = TRUE; return TRUE;
    case '9': is_probe_filter=FALSE; is_dump = TRUE; return TRUE;
    case '0': is_dump = FALSE; return TRUE;
    case 'Z': dump_frame(sim_time,TRUE); return TRUE;
    break;
    }
  }
  // is this key recognized by the selected nodes?
  // try it in the various sensor/actuator layers
  if(physics->handle_key(key)) return TRUE;
  for(int i=0;i<dynamics.max_id();i++) {
    Layer* d = (Layer*)dynamics.get(i);
    if(d && d->handle_key(key)) return TRUE;
  }
  BOOL in_selection = FALSE;
  for(int i=0;i<selection.max_id();i++) {
    Device* d = (Device*)devices.get((long)selection.get(i));
    if(d) in_selection |= d->handle_key(key);
  }
  return FALSE;
}

BOOL SpatialComputer::handle_mouse(MouseEvent* mouse) {
  return FALSE;
}

SpatialComputer* vis_context;
extern double get_real_secs ();
#define FLASH_TIME 0.1 // time that a snap flashes the background
void SpatialComputer::visualize() {
#ifdef WANT_GLUT
  vis_context=this;
  physics->visualize();
  for(int i=0;i<dynamics.max_id();i++)
    { Layer* d = (Layer*)dynamics.get(i); if(d) d->visualize(); }
  for(int i=0;i<devices.max_id();i++)
    { Device* d = (Device*)devices.get(i); if(d) d->visualize(); }
  // show "photo flashes" when dumps have occured
  SECONDS time = get_real_secs();
  if(just_dumped) { just_dumped=FALSE; snap_vis_time = time; }
  BOOL flash = is_show_snaps && (time-snap_vis_time) < FLASH_TIME;
  palette->set_background(flash ? PHOTO_FLASH : BACKGROUND);
#endif // WANT_GLUT
}

// special render for OpenGL selecting mode
void SpatialComputer::render_selection() {
  vis_context=this;
  for(int i=0;i<devices.max_id();i++)
    { Device* d = (Device*)devices.get(i); if(d) d->render_selection(); }
}
void SpatialComputer::update_selection() {
  for(int i=0;i<devices.max_id();i++) // clear old selection bits
    { Device* d = (Device*)devices.get(i); if(d) d->is_selected=FALSE; }
  for(int i=0;i<selection.max_id();i++) { // set new selection bits
    int n = (long)selection.get(i);
    Device* d = (Device*)devices.get(n); 
    if(d) d->is_selected=TRUE;
  }
}
void SpatialComputer::drag_selection(flo* delta) {
  if(volume->dimensions()==2) { // project back onto the surface
    delta[2]=0;
  }
  if(delta[0]==0 && delta[1]==0 && delta[2]==0) return;
  for(int i=0;i<selection.max_id();i++) { // move each selected device
    int n = (long)selection.get(i);
    Device* d = (Device*)devices.get(n); 
    if(d) { 
      const flo* p = d->body->position(); // calc new position
      d->body->set_position(p[0]+delta[0],p[1]+delta[1],p[2]+delta[2]);
      for(int j=0;j<dynamics.max_id();j++) // move the device
        { Layer* dyn = (Layer*)dynamics.get(j); if(dyn) dyn->device_moved(d); }
    }
  }
}

BOOL SpatialComputer::evolve(SECONDS limit) {
  SECONDS dt = limit-sim_time;
  // evolve world
  physics->evolve(dt);
  for(int i=0;i<devices.max_id();i++) { // tell layers about moving devices
    Device* d = (Device*)devices.get(i);
    if(d && d->body->moved) {
      for(int j=0;j<dynamics.max_id();j++) 
        { Layer* dyn = (Layer*)dynamics.get(j); if(dyn) dyn->device_moved(d); }
      d->body->moved=FALSE;
    }
  }
  // evolve other layers
  for(int i=0;i<dynamics.max_id();i++) {
    Layer* d = (Layer*)dynamics.get(i);
    d->evolve(dt);
  }
  // evolve devices
  Event e; scheduler->set_bound(limit);
  while(scheduler->pop_next_event(&e)) {
    int id = (long)e.target;
    Device* d = (Device*)devices.get(id);
    if(d && d->uid==e.uid) {
      sim_time=e.true_time; // set time to new value
      hardware.set_vm_context(d); // align kernel/sim patch for this device
      d->internal_event(e.internal_time,(DeviceEvent)e.type);
      if(e.type==COMPUTE) {
        d->run_time = e.internal_time;
        SECONDS tt, it;  // true and internal time
        d->timer->next_compute(&tt,&it); tt+=sim_time; it+=d->run_time;
	scheduler->schedule_event((void*)id,tt,it,COMPUTE,d->uid);
        d->timer->next_transmit(&tt,&it); tt+=sim_time; it+=d->run_time;
        scheduler->schedule_event((void*)id,tt,it,BROADCAST,d->uid);
      }
    }
  }
  sim_time=limit;
  
  // clone or kill devices (at end of update period)
  while(!death_q.empty()) {
    int id = death_q.front(); death_q.pop(); // get next to kill
    Device* d = (Device*)devices.get(id);
    if(d) {
      // scheduled events for dead devices are ignored; need not be deleted
      if(d->is_selected) { // fix selection (if needed)
        for(int i=0;i<selection.max_id();i++) {
          int n = (long)selection.get(i);
          if(n==id) selection.remove(i);
        }
      }
      delete d;
      devices.remove(id);
    }
  }
  while(!clone_q.empty()) {
    CloneReq* cr = clone_q.front(); clone_q.pop(); // get next to clone
    Device* d = (Device*)devices.get(cr->id);
    if(d && d==cr->parent) { // check device: might have been deleted
      Device* new_d = d->clone_device(cr->child_pos);
      new_d->backptr = devices.add(new_d);
      if(new_d->is_selected) { selection.add((void*)new_d->backptr); }
      // schedule next event
      SECONDS tt, it;  // true and internal time
      new_d->timer->next_compute(&tt,&it); tt+=sim_time; it+=new_d->run_time;
      scheduler->schedule_event((void*)new_d->backptr,tt,it,COMPUTE,
                                new_d->uid);
    }
    delete cr;
  }
  
  // dump if needed
  if(is_dump && sim_time >= dump_start && sim_time >= next_dump) {
    dump_frame(next_dump,FALSE);
    while(next_dump <= sim_time) next_dump+=dump_period;
  }
  
  return TRUE;
}

/*****************************************************************************
 *  DUMPING FACILITY                                                         *
 *****************************************************************************/

void SpatialComputer::dump_selection(FILE* out, int verbosity) {
  for(int i=0;i<selection.max_id();i++) {
    int n = (long)selection.get(i);
    Device* d = (Device*)devices.get(n); if(d) d->dump_state(out,verbosity);
  }
}

void SpatialComputer::dump_state(FILE* out) {
  for(int i=0;i<devices.max_id();i++)
    { Device* d = (Device*)devices.get(i); if(d) d->dump_state(out,0); }
}

void SpatialComputer::dump_header(FILE* out) {
  fprintf(out,"% \"UID\" \"TICKS\" \"TIME\""); // device fields
  physics->dump_header(out);
  for(int i=0;i<dynamics.max_id();i++)
    { Layer* d = (Layer*)dynamics.get(i); if(d) d->dump_header(out); }
  if(is_dump_value) fprintf(out," \"OUT\"");
  fprintf(out,"\n");
}

void SpatialComputer::dump_frame(SECONDS time, BOOL time_in_name) {
  char buf[1000];
  // ensure that the directory exists
  snprintf(buf, 1000, "mkdir -p %s", dump_dir); system(buf);
  // open the file
  if(time_in_name)
    sprintf(buf,"%s/%s%.2f-%.2f.log",dump_dir,dump_stem,get_real_secs(),time);
  else
    sprintf(buf,"%s/%s%.2f.log",dump_dir,dump_stem,time);
  FILE* out = fopen(buf,"w");
  if(out==NULL) { post("Unable to open dump file '%s'\n",buf); return; }
  // output all the state
  dump_header(out); dump_state(out);
  fclose(out); // close the file
  just_dumped = TRUE; // prime drawing to flash
}