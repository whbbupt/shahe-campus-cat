/* Actual production LVGL/UI, simulated input/time/display; no hardware verdict. */
#include "lvgl.h"
#include "pet_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while (0)

static pet_model_t model;
static pet_result_t result;
static unsigned char frame[450][390][3], draw_buffer[390*50*3];

static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{
    int width=area->x2-area->x1+1;
    for(int y=area->y1;y<=area->y2;++y) for(int x=area->x1;x<=area->x2;++x) {
        assert(x>=0 && x<390 && y>=0 && y<450);
        unsigned i=((y-area->y1)*width+x-area->x1)*3;
        frame[y][x][0]=data[i+2]; frame[y][x][1]=data[i+1]; frame[y][x][2]=data[i];
    }
    lv_display_flush_ready(display);
}

static void clicked(pet_event_t event)
{
    result=pet_model_event(&model,event);
    const char *message=result==PET_LOW_FOOD?"Too hungry. Tap Eat first.":
        (result==PET_OK?NULL:"Take a short break first.");
    pet_ui_update(&model,message);
}

static lv_obj_t *find_button(const char *name)
{
    lv_obj_t *screen=lv_screen_active();
    for(unsigned i=0;i<lv_obj_get_child_count(screen);++i) {
        lv_obj_t *obj=lv_obj_get_child(screen,i);
        if(lv_obj_check_type(obj,&lv_button_class) &&
           !strcmp(lv_label_get_text(lv_obj_get_child(obj,0)),name)) return obj;
    }
    assert(!"button not found"); return NULL;
}

static lv_obj_t *find_cat(lv_obj_t *parent)
{
    for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *obj=lv_obj_get_child(parent,i);
        if(lv_obj_get_width(obj)==112 && lv_obj_get_height(obj)==96 &&
           lv_obj_has_flag(obj,LV_OBJ_FLAG_CLICKABLE)) return obj;
        lv_obj_t *found=find_cat(obj); if(found)return found;
    }
    return NULL;
}

static unsigned timers(void)
{
    unsigned n=0;
    for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;
    return n;
}

static void bounds(lv_obj_t *parent)
{
    lv_area_t outer; lv_obj_get_coords(parent,&outer);
    for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *obj=lv_obj_get_child(parent,i); lv_area_t a; lv_obj_get_coords(obj,&a);
        assert(a.x1>=outer.x1 && a.y1>=outer.y1 && a.x2<=outer.x2 && a.y2<=outer.y2);
        bounds(obj);
    }
}

static void screenshot(const char *name)
{
    fprintf(stderr,"[host:ui] rendering %s\n",name);
    lv_obj_update_layout(lv_screen_active()); bounds(lv_screen_active()); lv_refr_now(NULL);
    FILE *f=fopen(name,"wb"); assert(f); fprintf(f,"P6\n390 450\n255\n");
    assert(fwrite(frame,sizeof(frame),1,f)==1); fclose(f);
}

static void press(const char *name)
{
    lv_obj_send_event(find_button(name),LV_EVENT_CLICKED,NULL);
}

static void advance(uint32_t ms)
{
    pet_model_advance(&model,ms); pet_ui_update(&model,NULL);
}

int main(void)
{
    lv_init(); lv_display_t *display=lv_display_create(390,450);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    fprintf(stderr,"[host:ui] creating page\n");
    pet_model_init(&model); pet_ui_create(clicked); pet_ui_update(&model,NULL);
    screenshot("campus-dorm.ppm");
    unsigned expected_timers=timers();
    assert(find_cat(lv_screen_active()));
    press("Class"); assert(result==PET_OK && model.scene==PET_CLASSROOM && model.food==70 && model.mood==60);
    screenshot("campus-classroom.ppm");
    press("Canteen"); assert(result==PET_OK && model.scene==PET_CANTEEN && model.food==70);
    screenshot("campus-canteen.ppm");

    press("Study"); assert(result==PET_OK && model.scene==PET_CLASSROOM && model.food==65 && model.mood==65);
    assert(pet_model_view(&model)==PET_STUDYING);
    assert(lv_obj_has_state(find_button("Eat"),LV_STATE_DISABLED));
    assert(lv_obj_has_state(find_button("Dorm"),LV_STATE_DISABLED));
    screenshot("campus-study.ppm");
    advance(2500); screenshot("campus-study-progress.ppm");
    advance(2500); press("Study"); assert(result==PET_COOLDOWN && model.food==65 && model.mood==65);
    advance(3000); press("Eat");
    assert(result==PET_OK && model.scene==PET_CANTEEN && model.food==80 && pet_model_view(&model)==PET_FEEDING);
    screenshot("campus-eat.ppm");
    advance(2000);
    lv_obj_send_event(find_cat(lv_screen_active()),LV_EVENT_CLICKED,NULL);
    assert(result==PET_OK && model.mood==75 && pet_model_view(&model)==PET_PETTING);
    screenshot("campus-pet.ppm");
    advance(3000); press("Rest");
    assert(result==PET_OK && model.scene==PET_DORM && model.sleeping);
    assert(lv_obj_has_state(find_button("Study"),LV_STATE_DISABLED));
    assert(lv_obj_has_state(find_button("Canteen"),LV_STATE_DISABLED));
    screenshot("campus-rest.ppm");
    advance(60000); assert(model.mood==76);
    press("Wake"); assert(result==PET_OK && !model.sleeping);
    assert(!lv_obj_has_state(find_button("Study"),LV_STATE_DISABLED));
    press("Class");
    for(unsigned i=0;i<10;++i) {
        pet_ui_create(clicked); pet_ui_update(&model,NULL); lv_tick_inc(100); lv_timer_handler();
        assert(timers()==expected_timers && model.scene==PET_CLASSROOM && model.food==80 && model.mood==76);
    }
    lv_obj_send_event(find_cat(lv_screen_active()),LV_EVENT_CLICKED,NULL);
    assert(result==PET_OK && model.mood==86); advance(3000);
    model.food=9; model.mood=60; pet_ui_update(&model,NULL);
    press("Study"); assert(result==PET_LOW_FOOD && model.food==9 && model.mood==60 && !model.action_ms);
    screenshot("campus-low-food.ppm");
    model.food=0; model.mood=0; pet_ui_update(&model,NULL); screenshot("campus-zero.ppm");
    model.food=100; model.mood=100; pet_ui_update(&model,NULL); screenshot("campus-full.ppm");

    pet_model_init(&model); pet_ui_create(clicked); pet_ui_update(&model,NULL);
    /* Geometry is valid only after LVGL finishes the new page layout. */
    lv_obj_update_layout(lv_screen_active());
    lv_obj_t *cat=find_cat(lv_screen_active());
    assert(cat);
    const void *open=lv_image_get_src(lv_obj_get_child(cat,0));
    for(unsigned i=0;i<37;++i){lv_tick_inc(100);lv_timer_handler();}
    assert(lv_image_get_src(lv_obj_get_child(cat,0))!=open);
    screenshot("campus-blink.ppm");
    for(unsigned i=0;i<3;++i){lv_tick_inc(100);lv_timer_handler();}
    assert(lv_image_get_src(lv_obj_get_child(cat,0))==open);
    assert(timers()==expected_timers);
    puts("[host:ui] 3 campus scenes, study/eat/cat-pat/rest/wake, cooldown/low-food, 10 recreations, blink and nested bounds PASS; board not tested");
    return 0;
}
