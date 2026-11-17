#include "ina226_port.h"
#include <math.h>
#include <string.h>
int ina226_init(ina226_t *s,void *ctx,ina_read16_t read,ina_write16_t write,
                double r,double lsb) {
    memset(s,0,sizeof(*s));
    if(!read||!write||!isfinite(r)||!isfinite(lsb)||r<=0||lsb<=0)return -1;
    double cal=floor(.00512/(r*lsb));
    if(cal<1||cal>32767)return -1;
    s->ctx=ctx;s->read=read;s->write=write;s->calibration=(uint16_t)cal;
    s->current_lsb_A=.00512/(r*cal);
    uint16_t manufacturer=0,device=0,check=0;
    if(read(ctx,0xFE,&manufacturer)||read(ctx,0xFF,&device))return -1;
    if(manufacturer!=0x5449||(device&0xFFF0)!=0x2260)return -1;
    if(write(ctx,0x00,0x4000)||write(ctx,0x05,s->calibration))return -1;
    if(read(ctx,0x05,&check)||check!=s->calibration)return -1;
    s->initialized=1;return 0;
}
int ina226_trigger(ina226_t *s) {
    if(!s->initialized)return -1;
    /* AVG=1; shunt then bus, 140us each; triggered shunt+bus, MODE=011. */
    return s->write(s->ctx,0x00,0x4003)?-1:0;
}
int ina226_read_pair(ina226_t *s,double *v,double *i) {
    if(!s->initialized)return -1;
    uint16_t status,bus,current;
    if(s->read(s->ctx,0x06,&status))return -1;
    if(status&0x0004)return -2;
    if(!(status&0x0008))return 1;
    if(s->read(s->ctx,0x02,&bus)||s->read(s->ctx,0x04,&current))return -1;
    *v=(double)bus*.00125;
    int signed_current=current>=32768?(int)current-65536:(int)current;
    *i=(double)signed_current*s->current_lsb_A;
    return 0;
}
