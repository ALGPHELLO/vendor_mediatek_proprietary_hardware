
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <net/if_arp.h>		    /* For ARPHRD_ETHER */
#include <sys/socket.h>		    /* For AF_INET & struct sockaddr */
#include <netinet/in.h>         /* For struct sockaddr_in */
#include <netinet/if_ether.h>
#include <linux/wireless.h>


#include <unistd.h>
#include <asm/types.h>
#include <sys/socket.h>

#include <net/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <cutils/log.h>


#include "iwlib.h"
#include "wifi_conn_api.h"


#define LOAD_WIFI_MODULE_ONCE


//#define LOGE printf
//#define LOGD printf
#define LOGE ALOGE
#define LOGD ALOGD


static int skfd = -1;
static int  sPflink = -1;

int	iw_ignore_version = 0;


typedef struct ap_info{
    char ssid[33];
    unsigned char mac[6];
    int mode;    
    int channel;    
    unsigned int rssi;
    int rate;
    int media_status;	
}ap_info;

enum{
    media__disconnect=0,
    media_connecting,
    media_connected
}media_status;


/***********************************************************************/

int
iw_get_range_info(int		skfd,
		  const char *	ifname,
		  iwrange *	range)
{
  struct iwreq		iw;
  char			buffer[sizeof(iwrange) * 2];	/* Large enough */
  union iw_range_raw *	range_raw;

  /* Cleanup */
  memset(buffer, 0, sizeof(buffer));

  iw.u.data.pointer = (caddr_t) buffer;
  iw.u.data.length = sizeof(buffer);
  iw.u.data.flags = 0;
  if(my_iw_get_ext(skfd, ifname, SIOCGIWRANGE, &iw) < 0)
    return(-1);

  /* Point to the buffer */
  range_raw = (union iw_range_raw *) buffer;

  /*to check the version directly */
  if(iw.u.data.length < 300)
    {
      range_raw->range.we_version_compiled = 9;
    }

  if(range_raw->range.we_version_compiled > 15)
    {
      memcpy((char *) range, buffer, sizeof(iwrange));
    }
  else
    {
      memset((char *) range, 0, sizeof(struct iw_range));

      memcpy((char *) range,
	     buffer,
	     iwr15_off(num_channels));
	 LOGD("get channels num\n");
	 
      memcpy((char *) range + iwr_off(num_channels),
	     buffer + iwr15_off(num_channels),
	     iwr15_off(sensitivity) - iwr15_off(num_channels));
	LOGD("get sens\n");
	
      memcpy((char *) range + iwr_off(sensitivity),
	     buffer + iwr15_off(sensitivity),
	     iwr15_off(num_bitrates) - iwr15_off(sensitivity));
	  LOGD("get bit rate\n");
	  

      memcpy((char *) range + iwr_off(num_bitrates),
	     buffer + iwr15_off(num_bitrates),
	     iwr15_off(min_rts) - iwr15_off(num_bitrates));
	  
      /* Number of bitrates has changed, put it after */
      memcpy((char *) range + iwr_off(min_rts),
	     buffer + iwr15_off(min_rts),
	     iwr15_off(txpower_capa) - iwr15_off(min_rts));
	  LOGD("iw get range step 1\n");
	  

      memcpy((char *) range + iwr_off(txpower_capa),
	     buffer + iwr15_off(txpower_capa),
	     iwr15_off(txpower) - iwr15_off(txpower_capa));
	  LOGD("iw get range step 2\n");
	  
     
      memcpy((char *) range + iwr_off(txpower),
	     buffer + iwr15_off(txpower),
	     iwr15_off(avg_qual) - iwr15_off(txpower));
     	  LOGD("iw get range step 3\n");
		  
      memcpy((char *) range + iwr_off(avg_qual),
	     buffer + iwr15_off(avg_qual),
	     sizeof(struct iw_quality));
	  	  LOGD("iw get range step 4\n");
    }

  if(!iw_ignore_version)
    {
      if(range->we_version_compiled <= 10)
	{
	  LOGD("iw get range step 5\n");
	}

      if(range->we_version_compiled > WE_MAX_VERSION)
	{
	  	  LOGD("iw get range step 6\n");
	}

      if((range->we_version_compiled > 10) &&
	 (range->we_version_compiled < range->we_version_source))
	{
	  	  LOGD("iw get range step 7\n");
	}
    }
  iw_ignore_version = 1;

  return(0);
}

int
iw_get_basic_config(int			skfd,
		    const char *	ifname,
		    wireless_config *	info)
{
  struct iwreq		wrq;

  memset((char *) info, 0, sizeof(struct wireless_config));

  if(my_iw_get_ext(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
    /* If no wireless name : no wireless extensions */
    return -1;
  else
    {
      strncpy(info->name, wrq.u.name, IFNAMSIZ);
      info->name[IFNAMSIZ] = 0;
	  LOGD("iw get basic confg ifname=%s\n", info->name);
    }
  
  	wrq.u.data.pointer = (caddr_t) info->key;
	wrq.u.data.length = IW_ENCODING_TOKEN_MAX;
	wrq.u.data.flags = 0;
	if(my_iw_get_ext(skfd, ifname, SIOCGIWENCODE, &wrq) >= 0)
	  {
		info->has_key = 1;
		info->key_size = wrq.u.data.length;
		info->key_flags = wrq.u.data.flags;
	  }
	
	wrq.u.essid.pointer = (caddr_t) info->essid;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;
    if(my_iw_get_ext(skfd, ifname, SIOCGIWESSID, &wrq) >= 0)
	{
	  info->has_essid = 1;
	  info->essid_on = wrq.u.data.flags;
	}

  if(my_iw_get_ext(skfd, ifname, SIOCGIWFREQ, &wrq) >= 0)
    {
      info->has_freq = 1;
      info->freq = iw_freq2float(&(wrq.u.freq));
      info->freq_flags = wrq.u.freq.flags;
	  LOGD("iw get basic confg 2\n");
    }

  if(my_iw_get_ext(skfd, ifname, SIOCGIWMODE, &wrq) >= 0)
    {
      info->has_mode = 1;
      /* Note : event->u.mode is unsigned, no need to check <= 0 */
      if(wrq.u.mode < IW_NUM_OPER_MODE)
	info->mode = wrq.u.mode;
      else
	info->mode = IW_NUM_OPER_MODE;	/* Unknown/bug */
    }
  
  if(my_iw_get_ext(skfd, ifname, SIOCGIWNWID, &wrq) >= 0)
	{
		info->has_nwid = 1;
		memcpy(&(info->nwid), &(wrq.u.nwid), sizeof(iwparam));
		LOGD("iw get basic confg 3\n");
	}

  return(0);
}

int
iw_get_stats(int		skfd,
	     const char *	ifname,
	     iwstats *		stats,
	     const iwrange *	range,
	     int		has_range)
{
	struct iwreq		wrq;
	char	buf[256];
	char *	bp;
	int	t;
	int ret = -1;
	FILE *fp = NULL;
  /*detect condition properly */
  if((has_range) && (range->we_version_compiled > 11))
    {
      wrq.u.data.pointer = (caddr_t) stats;
      wrq.u.data.length = sizeof(struct iw_statistics);
      wrq.u.data.flags = 1;
	  
      strncpy(wrq.ifr_name, ifname, IFNAMSIZ);
      if(my_iw_get_ext(skfd, ifname, SIOCGIWSTATS, &wrq) < 0) {
		goto done;
      }
      ret = 0;
	  goto done;
    }
  else
    {
      fp = fopen(PROC_NET_WIRELESS, "r");

      if(fp==NULL) {
	  	goto done;
      }

      while(fgets(buf,255,fp))
	{
		int if_match = -1;
		int contain_comm = -1;
		
		bp=buf;
		while(*bp&&isspace(*bp))
			bp++;
	  
	  	LOGD("wireless entry: %s\n", bp);
		
		if_match = strncmp(bp,ifname,strlen(ifname));
		contain_comm = bp[strlen(ifname)]==':';

	  if(if_match && contain_comm)
	    {
	      /* Skip ethxyz: */
	      bp=strchr(bp,':');
	      bp++;
	      /*  status  */
	      bp = strtok(bp, " ");
	      sscanf(bp, "%X", &t);
		  LOGD("status:%d\n", t);
	      stats->status = (unsigned short) t;
		  
	      /* link */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL) {
			stats->qual.updated |= 1;
			LOGD("qual.update=%u", stats->qual.updated);
	      }
	      sscanf(bp, "%d", &t);
		  LOGD("link: %d\n", t);
	      stats->qual.qual = (unsigned char) t;
		  
	      /*  signal */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL) {
			stats->qual.updated |= 2;
			LOGD("qual.update=%u", stats->qual.updated);
	      }
	      sscanf(bp, "%d", &t);
		  LOGD("signal=%d\n", t);
	      stats->qual.level = (unsigned char) t;
		  
	      /* Noise */
	      bp = strtok(NULL, " ");
	      if(strchr(bp,'.') != NULL) {
			stats->qual.updated += 4;
			LOGD("qual.update=%u", stats->qual.updated);
	    	}
	      sscanf(bp, "%d", &t);
		  LOGD("Noise=%d\n", t);
	      stats->qual.noise = (unsigned char) t;
		  
		  LOGD("show discard packets\n");
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.nwid);
		  LOGD("nwid=%d\n", stats->discard.nwid);
		  
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.code);
		  LOGD("code=%d\n", stats->discard.code);
		  
	      bp = strtok(NULL, " ");
	      sscanf(bp, "%d", &stats->discard.misc);
		  LOGD("misc=%d\n", stats->discard.misc);
		  
	      fclose(fp);

	      ret = 0;
		  goto done;
	    }
	}
      fclose(fp);
      ret = -1;
	  goto done;
    }
done:
	return ret;
}

static int
mtk_get_iwinfo(int			skfd,
	 char *			ifname,
	 struct wireless_info *	info)
{
  struct ifreq ifr;
  struct iwreq		wrq;

  printf("mtk get iw info\n");
  memset((char *) info, 0, sizeof(struct wireless_info));
  if(iw_get_basic_config(skfd, ifname, &(info->b)) < 0)
	{
	  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	  if(ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
	  	LOGD("SIOCGIFFLAGS fail\n");
		return(-ENODEV);
	  }
	  else {
	  	LOGD("IF exist, but not support iw\n");
		return(-ENOTSUP);
	  }
	}

  if(iw_get_range_info(skfd, ifname, &(info->range)) >= 0) {
    info->has_range = 1;
  }
  /* Get Power Management settings */
  wrq.u.power.flags = 0;
  if(my_iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) >= 0)
    {
      info->has_power = 1;
      memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
	  LOGD("iw has power");
    }
  
  /* Get bit rate */
	if(my_iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) >= 0)
	  {
	  	LOGD("iw bitrate\n");
		info->has_bitrate = 1;
		memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
	  }

  /* Get AP address */
  if(my_iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) >= 0)
    {
      info->has_ap_addr = 1;
      memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof (sockaddr));
	  printf("iw AP addr\n");
    }

  /* Get stats */
  if(iw_get_stats(skfd, ifname, &(info->stats),
		  &info->range, info->has_range) >= 0)
    {
      info->has_stats = 1;
	  printf("iw get stats\n");
    }

#ifndef WE_ESSENTIAL
	if(my_iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) >= 0)
	  {
		info->has_frag = 1;
		LOGD("iw get frag\n");
		memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
	  }

	if((info->has_range) && (info->range.we_version_compiled > 9))
	 {
	   if(my_iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) >= 0)
		 {
		   info->has_txpower = 1;
		   LOGD("iw get tx_power\n");
		   memcpy(&(info->txpower), &(wrq.u.txpower), sizeof(iwparam));
		 }
	 }
	if((info->has_range) && (info->range.we_version_compiled > 10))
	   {
		 if(my_iw_get_ext(skfd, ifname, SIOCGIWRETRY, &wrq) >= 0)
	   {
		 info->has_retry = 1;
		 LOGD("iw get has tetry\n");
		 memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
	   }
	   }

	wrq.u.essid.pointer = (caddr_t) info->nickname;
	wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	wrq.u.essid.flags = 0;
	if(my_iw_get_ext(skfd, ifname, SIOCGIWNICKN, &wrq) >= 0)
	if(wrq.u.data.length > 1) {
	  info->has_nickname = 1;
	  LOGD("iw get nickname\n");
	}

	if(my_iw_get_ext(skfd, ifname, SIOCGIWSENS, &wrq) >= 0)
	{
	  info->has_sens = 1;
	  LOGD("iw get sens\n");
	  memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
	}

  if(my_iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) >= 0)
    {
      info->has_rts = 1;
      memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
	  LOGD("iw get rts\n");
    }
#endif

  return(0);
}




int FM_WIFI_init(void)
{
    struct sockaddr_nl local;	
    LOGD("[FM_WIFI_init]++\n");   

    if ((skfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        LOGE("[FM_WIFI_init] failed to open net socket\n");
        return -1;
    }  
    
    sPflink = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sPflink < 0) {
		LOGE("[FM_WIFI_init] failed socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE) errno=%d", errno);
        close(skfd);
        skfd = -1;
        return -1;
    }
    
    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;
    if (bind(sPflink, (struct sockaddr *) &local, sizeof(local)) < 0) {
        LOGE("[FM_WIFI_init] failed bind(netlink)");
        close(skfd);
        skfd = -1;
        close(sPflink);
        sPflink = -1;
        return -1;
    }
    
    return 0;
}

int open_wifi(void){

	LOGD("open wifi driver\n");
	if( FM_WIFI_init() < 0) {
		LOGD("[WIFI] FM_WIFI_init failed!\n");
		return -1;
	}

	return 0;
}

int get_rssi_init(void){

	int ret = -1;

	ret = open_wifi();

	if(0!=ret){
		get_rssi_deinit();
		return -1;
		
	}	
	return 0;
}

int FM_WIFI_deinit(void)
{
    close(skfd);
    skfd = -1;
    close(sPflink);
    sPflink = -1;
    return 0;
	//wifi_unload_driver();
}

int get_rssi_deinit(void){
	FM_WIFI_deinit();
	LOGD("close_wifi success.\n");
	return 0;
}

int get_rssi_connected(void){

    wireless_info	wlan_info;
	int rssi = 0;
            
	if( mtk_get_iwinfo( skfd, "wlan0", &wlan_info) < 0 ) {
        LOGE("[wifi_update_status] failed to get wlan0 info!\n");
        return -9999;	        	        
    }

	rssi = (unsigned int)(wlan_info.stats.qual.level) - 0x100;
	
	return rssi;
}

void
iw_init_event_stream(struct stream_descr *	stream,	/* Stream of events */
		     char *			data,
		     int			len)
{
  /* Cleanup */
  memset((char *) stream, '\0', sizeof(struct stream_descr));

  /* Set things up */
  stream->current = data;
  stream->end = data + len;
}

int
iw_extract_event_stream(struct stream_descr *	stream,	/* Stream of events */
			struct iw_event *	iwe,	/* Extracted event */
			int			we_version)
{
    const struct iw_ioctl_description *	descr = NULL;
    int		event_type = 0;
    unsigned int	event_len = 1;
    char *	pointer;
    unsigned	cmd_index;
	int ret = -1;
	
    if((stream->current + IW_EV_LCP_PK_LEN) > stream->end) {
		ret = 0;
        goto done;
    }
    memcpy((char *) iwe, stream->current, IW_EV_LCP_PK_LEN);

    if(iwe->len <= IW_EV_LCP_PK_LEN) {
        ret = -2;
		goto done;
    }

    if(iwe->cmd <= SIOCIWLAST)
    {
        cmd_index = iwe->cmd - SIOCIWFIRST;
        if(cmd_index < standard_ioctl_num) {
	  		descr = &(standard_ioctl_descr[cmd_index]);
			LOGD("EXTRACT EVENT step 1\n");
        }
    }
    else
    {
        cmd_index = iwe->cmd - IWEVFIRST;
        if(cmd_index < standard_event_num) {
	       descr = &(standard_event_descr[cmd_index]);
			LOGD("EXTRACT EVENT step 2\n");
        }
    }
    if(descr != NULL) {
      event_type = descr->header_type;
	  LOGD("EXTRACT EVENT step 3\n");
    }
	
    event_len = event_type_size[event_type];
	
    if((we_version <= 18) && (event_type == IW_HEADER_TYPE_POINT)) {
		LOGD("EXTRACT EVENT step 4\n");
    	event_len += IW_EV_POINT_OFF;
    }

    if(event_len <= IW_EV_LCP_PK_LEN)
      {
      	LOGD("EXTRACT EVENT step 5\n");
        stream->current += iwe->len;
        ret = 2;
		goto done;
      }
    event_len -= IW_EV_LCP_PK_LEN;
    
    if(stream->value != NULL) {
	  LOGD("EXTRACT EVENT step 6\n");
      pointer = stream->value;
    }
    else {
	  LOGD("EXTRACT EVENT step 7\n");
      pointer = stream->current + IW_EV_LCP_PK_LEN;
    }

  if((pointer + event_len) > stream->end)
    {
      stream->current += iwe->len;
	  LOGD("EXTRACT EVENT step 8\n");
      ret = -3;
	  goto done;
    }

  if((we_version > 18) && (event_type == IW_HEADER_TYPE_POINT)) {
  	LOGD("EXTRACT EVENT step 9\n");
    memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
	   pointer, event_len);
  }
  else {
    memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
	LOGD("EXTRACT EVENT step 10\n");
  }
  pointer += event_len;

  if(event_type == IW_HEADER_TYPE_POINT)
    {
      unsigned int	extra_len = iwe->len - (event_len + IW_EV_LCP_PK_LEN);
	  LOGD("EXTRACT EVENT step 11\n");
      if(extra_len > 0)
	{
	  iwe->u.data.pointer = pointer;
	  LOGD("EXTRACT EVENT step 12\n");
	  if(descr == NULL)
	    iwe->u.data.pointer = NULL;
	  else
	    {
	      unsigned int	token_len = iwe->u.data.length * descr->token_size;
			LOGD("EXTRACT EVENT step 13\n");
	      if((token_len != extra_len) && (extra_len >= 4))
		{
		  __u16		alt_dlen = *((__u16 *) pointer);
		  unsigned int	alt_token_len = alt_dlen * descr->token_size;
		  if((alt_token_len + 8) == extra_len)
		    {
		    	LOGD("EXTRACT EVENT step 14\n");
		      pointer -= event_len;
		      pointer += 4;
		      memcpy((char *) iwe + IW_EV_LCP_LEN + IW_EV_POINT_OFF,
			     pointer, event_len);
		      pointer += event_len + 4;
			  LOGD("EXTRACT EVENT step 15\n");
		      iwe->u.data.pointer = pointer;
		      token_len = alt_token_len;
		    }
		}

	      if(token_len > extra_len) {
		  	LOGD("EXTRACT EVENT step 17\n");
			iwe->u.data.pointer = NULL;
	      }
		  
	      if((iwe->u.data.length > descr->max_tokens)
		 && !(descr->flags & IW_DESCR_FLAG_NOMAX)) {
			iwe->u.data.pointer = NULL;
			LOGD("EXTRACT EVENT step 18\n");
	      }
		  
	      if(iwe->u.data.length < descr->min_tokens) {
		  	LOGD("EXTRACT EVENT step 19\n");
			iwe->u.data.pointer = NULL;
	      }
	    }
	}
      else {
		iwe->u.data.pointer = NULL;
		LOGD("EXTRACT EVENT step 20\n");
      }
      stream->current += iwe->len;
    }
  else
    {
      if(1 && ((((iwe->len - IW_EV_LCP_PK_LEN) % event_len) == 4)
	     || ((iwe->len == 12) && ((event_type == IW_HEADER_TYPE_UINT) ||
				      (event_type == IW_HEADER_TYPE_QUAL))) ) && 
				      (stream->value == NULL))
	{
	  LOGD("DBG - alt iwe->len = %d\n", iwe->len - 4);
	  pointer -= event_len;
	  pointer += 4;
	  memcpy((char *) iwe + IW_EV_LCP_LEN, pointer, event_len);
	  pointer += event_len;
	}

    if((pointer + event_len) <= (stream->current + iwe->len)) {
		LOGD("extrace event step 21\n");
		stream->value = pointer;
    }
      else
	{
	  stream->value = NULL;
	  stream->current += iwe->len;
	  LOGD("extrace event step 21\n");
	}
    }
  ret = 1;
  goto done;
done:
	return ret;
}

double
iw_freq2float(const iwfreq *	in)
{
#ifdef WE_NOLIBM
  /* Version without libm : slower */
  int		i;
  double	res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
#else	/* WE_NOLIBM */
  /* Version with libm : faster */
  return ((double) in->m) * pow(10,in->e);
#endif	/* WE_NOLIBM */
}


static inline struct wireless_scan *
iw_process_scanning_token(struct iw_event *		event,
			  struct wireless_scan *	wscan)
{
  struct wireless_scan *	oldwscan;

  /* Now, let's decode the event */
  switch(event->cmd)
    {
    case SIOCGIWAP:
      /* New cell description. Allocate new cell descriptor, zero it. */
      oldwscan = wscan;
      wscan = (struct wireless_scan *) malloc(sizeof(struct wireless_scan));
      if(wscan == NULL)
	return(wscan);
      /* Link at the end of the list */
      if(oldwscan != NULL)
	oldwscan->next = wscan;

      /* Reset it */
      memset(wscan, 0, sizeof(struct wireless_scan));

      /* Save cell identifier */
      wscan->has_ap_addr = 1;
      memcpy(&(wscan->ap_addr), &(event->u.ap_addr), sizeof (sockaddr));
      break;
    case SIOCGIWNWID:
      wscan->b.has_nwid = 1;
      memcpy(&(wscan->b.nwid), &(event->u.nwid), sizeof(iwparam));
      break;
    case SIOCGIWFREQ:
      wscan->b.has_freq = 1;
      wscan->b.freq = iw_freq2float(&(event->u.freq));
      wscan->b.freq_flags = event->u.freq.flags;
      break;
    case SIOCGIWMODE:
      wscan->b.mode = event->u.mode;
      if((wscan->b.mode < IW_NUM_OPER_MODE) && (wscan->b.mode >= 0))
	wscan->b.has_mode = 1;
      break;
    case SIOCGIWESSID:
      wscan->b.has_essid = 1;
      wscan->b.essid_on = event->u.data.flags;
      memset(wscan->b.essid, '\0', IW_ESSID_MAX_SIZE+1);
      if((event->u.essid.pointer) && (event->u.essid.length))
	memcpy(wscan->b.essid, event->u.essid.pointer, event->u.essid.length);
      break;
    case SIOCGIWENCODE:
      wscan->b.has_key = 1;
      wscan->b.key_size = event->u.data.length;
      wscan->b.key_flags = event->u.data.flags;
      if(event->u.data.pointer)
	memcpy(wscan->b.key, event->u.essid.pointer, event->u.data.length);
      else
	wscan->b.key_flags |= IW_ENCODE_NOKEY;
      break;
    case IWEVQUAL:
      /* We don't get complete stats, only qual */
      wscan->has_stats = 1;
      memcpy(&wscan->stats.qual, &event->u.qual, sizeof(struct iw_quality));
      break;
    case SIOCGIWRATE:
      /* Scan may return a list of bitrates. As we have space for only
       * a single bitrate, we only keep the largest one. */
      if((!wscan->has_maxbitrate) ||
	 (event->u.bitrate.value > wscan->maxbitrate.value))
	{
	  wscan->has_maxbitrate = 1;
	  memcpy(&(wscan->maxbitrate), &(event->u.bitrate), sizeof(iwparam));
	}
    case IWEVCUSTOM:
      /* How can we deal with those sanely ? Jean II */
    default:
      break;
   }	/* switch(event->cmd) */

  return(wscan);
}



int
iw_process_scan(int			skfd,
		char *			ifname,
		int			we_version,
		wireless_scan_head *	context)
{
  struct iwreq		wrq;
  unsigned char *	buffer = NULL;		/* Results */
  int			buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  unsigned char *	newbuf;

  /* Don't waste too much time on interfaces (150 * 100 = 15s) */
  context->retry++;
  if(context->retry > 150)
    {
      errno = ETIME;
      return(-1);
    }
#if 0
  /* If we have not yet initiated scanning on the interface */
  if(context->retry == 1)
    {
      /* Initiate Scan */
      wrq.u.data.pointer = NULL;		/* Later */
      wrq.u.data.flags = 0;
      wrq.u.data.length = 0;
      /* Remember that as non-root, we will get an EPERM here */
      if((iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0)
	 && (errno != EPERM))
	return(-2);
      /* Success : now, just wait for event or results */
      return(1500);	/* Wait 250 ms */
    }
#endif

 realloc:
  /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
  newbuf = realloc(buffer, buflen);
  if(newbuf == NULL)
    {
      /* man says : If realloc() fails the original block is left untouched */
      if(buffer)
	free(buffer);
      errno = ENOMEM;
      return(-3);
    }
  buffer = newbuf;

  /* Try to read the results */
  wrq.u.data.pointer = buffer;
  wrq.u.data.flags = 0;
  wrq.u.data.length = buflen;
  if(my_iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0)
    {
      /* Check if buffer was too small (WE-17 only) */
      if((errno == E2BIG) && (we_version > 16))
	{
	  if(wrq.u.data.length > buflen)
	    buflen = wrq.u.data.length;
	  else
	    buflen *= 2;

	  /* Try again */
	  goto realloc;
	}

      /* Check if results not available yet */
      if(errno == EAGAIN || wrq.u.data.length == 0)
	{
	  free(buffer);
	  /* Wait for only 100ms from now on */
	  //return(100);	/* Wait 100 ms */
	  return(-6);
	}

      free(buffer);
      /* Bad error, please don't come back... */
      return(-4);
    }

    //LOGD("[iw_process_scan] errno=%d length=%d\n", errno, wrq.u.data.length);
  /* We have the results, process them */
  if(wrq.u.data.length)
    {
      struct iw_event		iwe;
      struct stream_descr	stream;
      struct wireless_scan *	wscan = NULL;
      int    ret = -10;
#if 0
      /* Debugging code. In theory useless, because it's debugged ;-) */
      int	i;
      printf("Scan result [%02X", buffer[0]);
      for(i = 1; i < wrq.u.data.length; i++)
	printf(":%02X", buffer[i]);
      printf("]\n");
#endif

      memset(&iwe, 0, sizeof(struct iw_event));
      /* Init */
      iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
      /* This is dangerous, we may leak user data... */
      context->result = NULL;

      /* Look every token */
      do
	{
	  /* Extract an event and print it */
	  ret = iw_extract_event_stream(&stream, &iwe, we_version);
	  if(ret > 0)
	    {
	      /* Convert to wireless_scan struct */
	      wscan = iw_process_scanning_token(&iwe, wscan);
	      /* Check problems */
	      if(wscan == NULL)
		{
		  free(buffer);
		  errno = ENOMEM;
		  return(-5);
		}
	      /* Save head of list */
	      if(context->result == NULL)
		context->result = wscan;
	    }
	}
      while(ret > 0);
    }
   else
    {
        free(buffer);
	    /* Wait for only 100ms from now on */
	    //return(100);	/* Wait 100 ms */
		return(-7);
    }

    /* Done with this interface - return success */
    free(buffer);
    return (0);
}


int
iw_scan(int			skfd,
    char *			ifname,
    int			we_version,
    wireless_scan_head *	context)
{
    int		delay;		/* in ms */
    
    /* Clean up context. Potential memory leak if(context.result != NULL) */
    context->result = NULL;
    context->retry = 0;
    
    /* Wait until we get results or error */
    while(1){
        /* Try to get scan results */
        delay = iw_process_scan(skfd, ifname, we_version, context);
    
        /* Check termination */
        if(delay <= 0)
            break;
    
        /* Wait a bit */
        usleep(delay * 1000);
    }

    LOGD("[iw_scan] delay=%d context->retry=%d\n", delay, context->retry); 
    
    /* End - return -1 or 0 */
    return(delay);
}

int get_rssi_unconnected(void){

	wireless_scan_head scanlist;
	wireless_scan *	item;	
	int max_rssi = 0;
	int rssi = 0;
	int i = 0;

	if( iw_scan(skfd, "wlan0", 21, &scanlist) <0 ) {
        LOGE("meta failed to get scan list!\n");   
        return -9999;
    }

	if( scanlist.result == NULL ){
        LOGE("no scan result!\n");   
        return -9999;
    }

	max_rssi = -9999;

	for(item = scanlist.result; item!= NULL ;item=item->next){

		if(item->b.has_essid && (strcmp( AP_SSID, item->b.essid) == 0) ) {

			i++;
			rssi = item->stats.qual.level - 0x100;
			LOGD("====%d rssi = %d!\n",i,rssi);
			if(rssi > max_rssi)
				max_rssi = rssi;
		}
	}

	if(-9999 != max_rssi){
		LOGD("====max rssi = %d total num %d\n",max_rssi,i);
		return max_rssi;
	}

	return -9999;
}

