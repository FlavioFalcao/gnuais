#include <gtk/gtk.h>
#include <stdio.h>
#include <pthread.h>
#include <glib.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

#define DBG(x) 
#define MAXSHIPS 1000


typedef struct {
	GtkWidget *drawing;
	GtkWidget *textframe;
} threadwidgets;

typedef struct {
	int mapwidth;
	int mapheight;
	double topleftlon;
	double topleftlat;
	double botrightlon;
	double botrightlat;
} mapcoord;
mapcoord mapcoords;

typedef struct {
	int mmsi;
	double longitude;
	double latitude;
	float heading;
	float coarse;
	int type;
} ship;

typedef struct {
	int type;
	int mmsi;
	double latitude;
	double longitude;
	float speed;
	float coarse;
	float heading;
} shipdata;

static gboolean configure_event( GtkWidget         *widget,
                                 GdkEventConfigure *event );
void drawboats(GtkWidget *drawing_area);

static GdkPixmap *pixmap = NULL;
GdkPixbuf *map;
ship *ships[MAXSHIPS];
int numberofships=0;

char mapfilename[100] = "map.png";


G_LOCK_DEFINE_STATIC(updatemap);


unsigned int pickone(char *r_buffer,int pos,int len)
{
	int i;
	unsigned int tmp = 0;
	for(i=0;i<len;i++)
	{	
		tmp |= r_buffer[pos+i] << (len-1-i);
	}
	return tmp;
}


void type1(char *r_buffer,int lengde, shipdata *data)
{
	int i,j;
	unsigned int mmsi = pickone(r_buffer,8,30);
	int longitude=pickone(r_buffer,61,28);
	if(((longitude >> 27)&1)==1)
		longitude |= 0xF0000000;
	int latitude=pickone(r_buffer,89,27);
	unsigned int coarse=pickone(r_buffer,116,12);
	unsigned int heading=pickone(r_buffer,128,9);
	unsigned int sog=pickone(r_buffer,50,10);
	char rateofturn=pickone(r_buffer,40,8);
	
	printf("MMSI: %09d  Latitude: %.7f  Longitude: %.7f  Speed:%f  Coarse:%.5f  Heading: %f  Rateofturn: %d\n", mmsi,(float)latitude/600000,(float)longitude/600000,(float)sog/10,(float)coarse/10,(float)heading,rateofturn);
	data->type = 1;
	data->mmsi = mmsi;
	data->latitude = (float)latitude/600000;
	data->longitude = (float)longitude/600000;
	data->speed = (float)sog/10;
	data->heading = (float)heading;
	data->coarse = (float)coarse/10;
	
}

void type4(char *r_buffer,int lengde, shipdata *data)
{
	int i,j;
	unsigned int mmsi = pickone(r_buffer,8,30);
	unsigned int year = pickone(r_buffer,40,12);
	unsigned int month = pickone(r_buffer,52,4);
	unsigned int day = pickone(r_buffer,56,5);
	unsigned int hour = pickone(r_buffer,61,5);
	unsigned int minute = pickone(r_buffer,66,6);
	unsigned int second = pickone(r_buffer,72,6);
	int longitude = pickone(r_buffer,79,28);
	if(((longitude >> 27)&1)==1)
		longitude |= 0xF0000000;
	unsigned int latitude = pickone(r_buffer,107,27);

	printf("MMSI: %09d  Latitude: %.7f  Longitude: %.7f\n",mmsi,(float)latitude/600000,(float)longitude/600000);
	data->type = 4;
	data->mmsi = mmsi;
	data->latitude = (float)latitude/600000;
	data->longitude = (float)longitude/600000;

}

void type5(char *r_buffer,int lengde, shipdata *data)
{
	int i,j;
	unsigned int mmsi = pickone(r_buffer,8,30);
	int start = 112;
	char name[21];
	for(i=0;i<20;i++)
	{
		char letter = pickone(r_buffer,start,6);
		name[i] = letter + 64;
		if (name[i] == 64 || name[i] == 96) name[i] = ' ';
		start += 6;
	}
	name[20] = 0;
	start = 302;
	char destination[21];
	for(i=0;i<20;i++)
	{
		char letter = pickone(r_buffer,start,6);
		destination[i] = letter + 64;
		if (destination[i] == 64 || destination[i] == 96) destination[i] = ' ';
		start += 6;
	}
	destination[20] = 0;

	unsigned int A = pickone(r_buffer,240,9);
	unsigned int B = pickone(r_buffer,249,9);
	unsigned int C = pickone(r_buffer,258,6);
	unsigned int D = pickone(r_buffer,264,6);
	unsigned int height = pickone(r_buffer,294,8);

	printf("MMSI: %09d  Name: %s  Destination: %s  A=%d  B=%d  C=%d  D=%d  Height: %f\n",mmsi,name,destination,A,B,C,D,(float)height/10);
	data->type = 5;
	data->mmsi = mmsi;

}




void aisdecode(char *nmea, shipdata *data)
{
	int i;
	char bokstav;
	static char r_buffer[600];
	DBG(printf("nmealength %d\n",strlen(nmea)*6);)
	for(i=0;i<(strlen(nmea)*6);i++)
	{
	
		bokstav = (nmea[(i/6)]);
		
		if (bokstav == ',') break;
		if (bokstav < 87)
			bokstav = bokstav - 48;
		else
			bokstav = bokstav - 56;
		r_buffer[i] = (bokstav >> (5-(i%6))) & 0x01;
	}


	int lengde = i;
	int j;

	unsigned char type = pickone(r_buffer,0,6);
	printf("(%d) ",type);

	switch(type){
	case 1:
		type1(r_buffer,lengde,data);
	break;
	case 2:
		type1(r_buffer,lengde,data);
	break;
	case 3:
		type1(r_buffer,lengde,data);
	break;
	case 4:
		type4(r_buffer,lengde,data);
	break;
	case 5:
		type5(r_buffer,lengde,data);
	}
}


gboolean delete_event( GtkWidget *Widget, GdkEvent *event, gpointer data)
{
	gtk_widget_destroy(Widget);
	return 1;
}

void destroy(GtkWidget *Widget, gpointer data)
{
	gtk_main_quit();
}

void hello(GtkWidget *widget, gpointer data)
{
	GtkWidget *drawing_area = GTK_WIDGET(data);
	static int teller = 0;
	g_print("Changes map\n");
	if (teller % 3 == 0)
		changemap(drawing_area,"waterford2.png");
	else if (teller % 3 == 1)
		changemap(drawing_area,"waterford.png");
	else
		changemap(drawing_area,"map.png");
		
	teller++;

}

int initserial(const char *dev)
{
	int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1){ // Could not open the port.
		perror("open_port: Unable to open serial port");
		}		
	else {
 	    //printf("serial opened. My_fd is:%d\n",fd);
 	    fcntl(fd, F_SETFL, 0);
 
 	    struct termios options;
 	    /* get the current options */
 	    tcgetattr(fd, &options);
  
 	    //set speed 4800
 	    cfsetispeed(&options, B4800);
 	    cfsetospeed(&options, B4800);   
 	    /* set raw input, 1 second timeout */
 	    options.c_cflag     |= (CLOCAL | CREAD);
 	    options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
 	    options.c_oflag     &= ~OPOST;
 	    options.c_cc[VMIN]  = 0;
 	    options.c_cc[VTIME] = 10;
 
 	    //No parity 8N1
 	    options.c_cflag &= ~PARENB;
 	    options.c_cflag &= ~CSTOPB;
 	    options.c_cflag &= ~CSIZE;
 	    options.c_cflag |= CS8;		    
 	    /* set the options */
 	    tcsetattr(fd, TCSANOW, &options);
 	    }
	return fd;
}

void findPixel(double longitude,double latitude,GdkPoint *p)
{
	double difflon = mapcoords.botrightlon - mapcoords.topleftlon;
	double difflat = mapcoords.topleftlat - mapcoords.botrightlat;
	double pixels_per_lon = mapcoords.mapwidth / difflon;
	double pixels_per_lat = mapcoords.mapheight / difflat;
	p->x = (int)((longitude-mapcoords.topleftlon)*pixels_per_lon);
	p->y = (int)((mapcoords.topleftlat-latitude)*pixels_per_lat);
	if (p->x > mapcoords.mapwidth || p->x < 0) p->x = -1;
	if (p->y > mapcoords.mapheight || p->y < 0) p->y = -1;
}


void updateship(int mmsi, double longitude, double latitude, float heading, float coarse, int type)
{
	G_LOCK(updatemap);
		int i;
		for(i=0;i<numberofships;i++)
		{
			if (ships[i]->mmsi == mmsi){
				ships[i]->longitude = longitude;
				ships[i]->latitude = latitude;
				ships[i]->heading = heading;
				ships[i]->coarse = coarse;
				ships[i]->type = type;
				G_UNLOCK(updatemap);
				return;
			}
		}
		if(numberofships < MAXSHIPS){
			ships[numberofships] =  (ship*)malloc(sizeof(ship));
			ships[numberofships]->mmsi = mmsi;
			ships[numberofships]->longitude = longitude;
			ships[numberofships]->latitude = latitude;
			ships[numberofships]->heading = heading;
			ships[numberofships]->coarse = coarse;
			ships[numberofships]->type = type;
			numberofships++;
		}
		else printf("Too many ships saved\n");
	G_UNLOCK(updatemap);
}


void *threaden (void *args)
{
	threadwidgets *t = (threadwidgets*)args;
	int x,y,i;
	float theta;
	shipdata shipd;
	GtkWidget *widget = GTK_WIDGET(t->drawing);
	GtkWidget *nmeatext = GTK_WIDGET(t->textframe);
	int fd = initserial("/tmp/gnuaispipe");
	char nmeabuffer[201];
	GdkPoint p;
	int lettersread;
	int previoussentence=0;
	char aisline[500];
	char tmp = 0;
	int j,k;
	int sentences;
	int sentencenumb;
	int start;
	int kommas;
	int r = 0;
	int m = 0;
	while(1){

		usleep(500000);
		m=0;
		r = read(fd,nmeabuffer,200);
		nmeabuffer[r] = 0;
		DBG(	printf("<---- lest   %s  -------->\n",nmeabuffer);)
		tmp = 0;
		while(nmeabuffer[m++] != '!')
		{
			if (m > (r-1)) {
				break;
			}

		}
		if (m > (r-1)) continue;
		m--;

		 if (nmeabuffer[m] != '!' || nmeabuffer[m+1] != 'A' || nmeabuffer[m+5] != 'M') {continue;}
		kommas = 0;
		for(k=0;k<20;k++){
			if (nmeabuffer[k+m] == ',') kommas++;
			if (kommas == 5) break;
		}
		start = k+1;
		sentences = nmeabuffer[m+7]-0x30; 
		sentencenumb = nmeabuffer[m+9]-0x30;
		if ((sentencenumb > 1) && ((previoussentence) != (sentencenumb-1))) continue;
		if (sentencenumb == 1) lettersread = 0;
		for(j=0;j<(r-start-m);j++)
		{
					
			if (nmeabuffer[m+j+start] == ',') break;
			aisline[lettersread] = nmeabuffer[m+j+start];
			lettersread++;

		}

		if (sentencenumb >= sentences)
		{
			aisline[lettersread] = 0;
			aisdecode(aisline,&shipd);
		}

		previoussentence = sentencenumb;
		if(shipd.type != 1 && shipd.type != 2 && shipd.type != 3 && shipd.type != 4) continue;
		
		updateship(shipd.mmsi,shipd.longitude,shipd.latitude,shipd.heading,shipd.coarse,shipd.type);

		gdk_threads_enter();
		configure_event(widget,NULL);  // redraw map	
		gdk_threads_leave();

	}
}

static gboolean configure_event( GtkWidget         *widget,
                                 GdkEventConfigure *event )
{
  if (pixmap)
    g_object_unref (pixmap);
	GError *error = NULL;
	guchar *pixels;
	pixels = gdk_pixbuf_get_pixels(map);
	int rowstride = gdk_pixbuf_get_rowstride(map);
  pixmap = gdk_pixmap_new (widget->window,
			   widget->allocation.width,
			   widget->allocation.height,
			   -1);

  	gdk_draw_rectangle (pixmap, widget->style->white_gc,TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	gdk_draw_rgb_image(pixmap,widget->style->fg_gc[GTK_STATE_NORMAL],0,0,mapcoords.mapwidth,mapcoords.mapheight,GDK_RGB_DITHER_NORMAL,pixels,rowstride);

	drawboats(widget);

  return TRUE;
}

void drawboats(GtkWidget *drawing_area)
{
		int i;
	GdkPoint p;
	int x;
	int y;
	float theta;
	
	G_LOCK(updatemap);

	for(i=0;i<numberofships;i++)
	{

		findPixel(ships[i]->longitude,ships[i]->latitude,&p);
		x = p.x;
		y = p.y;
		if(ships[i]->heading != 511)
			theta = ships[i]->heading*M_PI/180; 
		else
			theta = ships[i]->coarse*M_PI/180; 
		
		printf("x:%d y:%d\n",x,y);
		if (x == -1 || y == -1) continue;

		GdkPoint punkter[3];
		GdkPoint punkter2[4];
		if (ships[i]->type != 4){
		punkter[0].x=x-6*cos(theta)+18*sin(theta);
		punkter[0].y=y-6*sin(theta)-18*cos(theta);
		punkter[1].x=x+0*cos(theta)+29*sin(theta);
		punkter[1].y=y+0*sin(theta)-29*cos(theta);
		punkter[2].x=x+5*cos(theta)+18*sin(theta);
		punkter[2].y=y+5*sin(theta)-18*cos(theta);
		
		punkter2[0].x=x-5*cos(theta)+20*sin(theta);
		punkter2[0].y=y-5*sin(theta)-20*cos(theta);
		punkter2[1].x=x+5*cos(theta)+20*sin(theta);
		punkter2[1].y=y+5*sin(theta)-20*cos(theta);
		punkter2[2].x=x+5*cos(theta)-10*sin(theta);
		punkter2[2].y=y+5*sin(theta)+10*cos(theta);
		punkter2[3].x=x-5*cos(theta)-10*sin(theta);
		punkter2[3].y=y-5*sin(theta)+10*cos(theta);	
 		gdk_draw_polygon(pixmap,drawing_area->style->black_gc,TRUE,punkter,3);//fg_gc[GTK_STATE_NORMAL]
		gdk_draw_polygon(pixmap,drawing_area->style->black_gc,TRUE,punkter2,4);//fg_gc[GTK_STATE_NORMAL
		}
		else {
		  gdk_draw_rectangle (pixmap,drawing_area->style->black_gc,  TRUE, x-10,y-10, 20, 20);

		}
	
		
	}
	gtk_widget_queue_draw_area (drawing_area, 				0, 0,				drawing_area->allocation.width,				drawing_area->allocation.height);

	G_UNLOCK(updatemap);

}

int changemap(GtkWidget *drawing_area, char *filename)
{
	char cbrfilename[100];
	map= gdk_pixbuf_new_from_file(filename,0);
	if(!map)
	{	printf("Can't open mapfile. Please stay in a directory with a image file called map.png\n");
		return 0;
	}
	strcpy(mapfilename,filename);
	mapcoords.mapwidth = gdk_pixbuf_get_width(map);
	mapcoords.mapheight = gdk_pixbuf_get_height(map);
	
	int strl = strlen(filename);
	strncpy(cbrfilename,mapfilename,strl-3);
	cbrfilename[strl-3] = 'c';
	cbrfilename[strl-2] = 'b';
	cbrfilename[strl-1] = 'r';
	cbrfilename[strl] = 0;
	FILE *cbrfile = fopen(cbrfilename,"r");
	if (cbrfile == NULL)
	{
		printf("Could not find calibrate file for map!!! You should make a file called %s in the same folder as the map.\n",cbrfilename);
		mapcoords.topleftlon = 0;
		mapcoords.topleftlat = 1;
		mapcoords.botrightlon = 1;
		mapcoords.botrightlat = 0;
	}
	else {
		fscanf(cbrfile,"%lf",&mapcoords.topleftlon);
		fscanf(cbrfile,"%lf",&mapcoords.topleftlat);
		fscanf(cbrfile,"%lf",&mapcoords.botrightlon);
		fscanf(cbrfile,"%lf",&mapcoords.botrightlat);
		fclose(cbrfile);
	}

	gtk_widget_set_size_request(GTK_WIDGET(drawing_area),mapcoords.mapwidth,mapcoords.mapheight);	
}

static gboolean expose_event( GtkWidget      *widget, GdkEventExpose *event )
{
  	gdk_draw_drawable (widget->window, widget->style->fg_gc[GTK_WIDGET_STATE (widget)],  pixmap, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

  return FALSE;
}
static gboolean button_press_event( GtkWidget      *widget,
                                    GdkEventButton *event )
{
  if (event->button == 1 && pixmap != NULL)
    {
}
  return TRUE;
}

int main(int argc, char **argv)
{
	srand(time(0));
	pthread_t thre;
	char cbrfilename[100];

	GtkWidget *window;
	GtkWidget *button, *button2;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *drawing_area;	
 	GtkWidget *scrolled_window;
	GtkWidget *notebook;
	GtkWidget *overviewframe;
	GtkWidget *nmeaframe;
	GtkWidget *settingsframe;
	GtkWidget *label;
	GtkWidget *label2;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *frame;
	GtkWidget *vboxmenu;
	

 	
	threadwidgets *twidgets = (threadwidgets*)malloc(sizeof(threadwidgets));

	g_type_init();
	map = gdk_pixbuf_new_from_file(mapfilename,0);
	if(!map)
	{	printf("Can't open mapfile. Please stay in a directory with a image file called map.png\n");
		exit(-1);
	}
	mapcoords.mapwidth = gdk_pixbuf_get_width(map);
	mapcoords.mapheight = gdk_pixbuf_get_height(map);
	
	int strl = strlen(mapfilename);
	strncpy(cbrfilename,mapfilename,strl-3);
	cbrfilename[strl-3] = 'c';
	cbrfilename[strl-2] = 'b';
	cbrfilename[strl-1] = 'r';
	cbrfilename[strl] = 0;
	FILE *cbrfile = fopen(cbrfilename,"r");
	if (cbrfile == NULL)
	{
		printf("Could not find calibrate file for map!!! You should make a file called %s in the same folder as the map.\n",cbrfilename);
		mapcoords.topleftlon = 0;
		mapcoords.topleftlat = 1;
		mapcoords.botrightlon = 1;
		mapcoords.botrightlat = 0;
	}
	else {
		fscanf(cbrfile,"%lf",&mapcoords.topleftlon);
		fscanf(cbrfile,"%lf",&mapcoords.topleftlat);
		fscanf(cbrfile,"%lf",&mapcoords.botrightlon);
		fscanf(cbrfile,"%lf",&mapcoords.botrightlat);
		fclose(cbrfile);
	}
	g_thread_init(NULL);
	gdk_threads_init();
	gdk_threads_enter();



	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(window),800,500);
	gtk_window_set_title(GTK_WINDOW(window), "GNU AIS Map GUI");
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width(GTK_CONTAINER (window), 0);

	button2 = gtk_button_new_with_label("Tull");

	drawing_area =  gtk_drawing_area_new();
	gtk_widget_set_size_request(GTK_WIDGET(drawing_area),mapcoords.mapwidth,mapcoords.mapheight);
	g_signal_connect (G_OBJECT (drawing_area), "expose_event", G_CALLBACK (expose_event), NULL);
	g_signal_connect (G_OBJECT (drawing_area),"configure_event", G_CALLBACK (configure_event), NULL);
	g_signal_connect (G_OBJECT (drawing_area), "button_press_event", G_CALLBACK (button_press_event), NULL);
	gtk_widget_set_events (drawing_area,  GDK_BUTTON_PRESS_MASK);

	scrolled_window = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window),10);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), drawing_area);

	button = gtk_button_new_with_label("Change map");
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(hello), (void*)drawing_area);

	overviewframe = gtk_frame_new("Listings");

	nmeaframe = gtk_frame_new("NMEA Sentences");

	settingsframe = gtk_frame_new(NULL);

	label = gtk_label_new("Map");

	label2 = gtk_label_new("Overview");

	label3 = gtk_label_new("Raw data");

	label4 = gtk_label_new("Settings");
	
	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window, label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), overviewframe, label2);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), nmeaframe, label3);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), settingsframe, label4);

	frame = gtk_frame_new(NULL);
	gtk_widget_set_size_request(GTK_WIDGET(frame),100,20);
;	
	hbox = gtk_hbox_new(0,0);
	gtk_box_pack_start(GTK_BOX(hbox),frame,0,0,0);
	gtk_box_pack_start(GTK_BOX(hbox),notebook,1,1,0);

	vbox = gtk_vbox_new(1,0);
	gtk_container_add(GTK_CONTAINER(window),vbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox,1,1,0);

	vboxmenu = gtk_vbox_new(0,0);
	gtk_container_add(GTK_CONTAINER (frame), vboxmenu);
	gtk_box_pack_start(GTK_BOX(vboxmenu),button,0,1,0);

	gtk_widget_show(button);
	gtk_widget_show(frame);
	gtk_widget_show(drawing_area);
	gtk_widget_show(vbox);
	gtk_widget_show(notebook);
	gtk_widget_show(overviewframe);
	gtk_widget_show(nmeaframe);
	gtk_widget_show(settingsframe);
	gtk_widget_show(label);
	gtk_widget_show(label2);
	gtk_widget_show(label3);
	gtk_widget_show(label4);
	gtk_widget_show(scrolled_window);
	gtk_widget_show(vboxmenu);
	gtk_widget_show(hbox);
	gtk_widget_show(window);

	twidgets->drawing = drawing_area;
	twidgets->textframe = NULL;
	pthread_create (&thre, NULL, threaden, twidgets);
	
	gtk_main();
	gdk_threads_leave();


	return 0;

}