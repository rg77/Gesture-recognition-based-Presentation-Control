#include<highgui.h>
#include<cxcore.h>
#include <opencv2/opencv.hpp>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include<unistd.h>
#include<iostream>
#include<math.h>

#define pi 3.1415

// The key code to be sent.
#define KEYCODE_1 XK_Right
#define KEYCODE_2 XK_Left
#define KEYCODE_3 XK_Up
#define KEYCODE_4 XK_Down

//Max value of HSV trackbar
#define val_max_Hue 179
#define val_max_Saturation 255
#define val_max_Value 255
# define max_size 31 //for median blur

using namespace cv;
using namespace std;


struct position
{
 int X;
 int Y;
}present,previous,first,last;

class idle
{
  bool status;
  short int frame_cnt;
  const short int max_frame_cnt = 1;
  public:

 idle()
 {
   status = 1;//1 means idle
   frame_cnt=-1;
 }

 void change_status()
 {
   if(status==0)
    status = 1;
   else
    status = 0;
 }


  bool get_status()
     {return status;}

  void inc_frame_cnt()
  {

      ++frame_cnt;
  }

  short int get_frame_cnt()
  {
        return frame_cnt;
  }

short int get_max_frame_cnt()
{
 return max_frame_cnt;
}

  void reset_frame_cnt()
  {
    frame_cnt = -1;
  }



}idle_ob;



float displacement(position p1,position p2)//to find displacement b/w 2 points
{
   return sqrt(   pow(p1.X - p2.X,2)  +  pow(p1.Y - p2.Y,2) );;
}

float mod(float x)
{
   if( x>= 0)
    return x;
   else
     return -x;
}


class sweep_gesture
{
 //bool status;
 float len_displacement;
 const float min_horizontal_sweep_len = 100;
 const float min_veertical_sweep_len = 100;
 const float max_deviation_angle = pi/6;
 public:


int detect_sweep(position first_pos,position last_pos)
{
   len_displacement = displacement(first_pos,last_pos);


   if(len_displacement >= min_horizontal_sweep_len )//sweep detected
   {

      if( ( mod(first_pos.X - last_pos.X) >= len_displacement*cos(max_deviation_angle) ) && ( mod(first_pos.Y - last_pos.Y) <= len_displacement*sin(max_deviation_angle)) )//horizontal sweep detected
      {
              if(first_pos.X < last_pos.X) //web camera captures stores the mirror image
               {
                  return 1;//right to leftt
               }
               else
               {
                   return 2;// left to right
               }


      }
   }

   else if(len_displacement >= min_veertical_sweep_len)
     {
          if( (mod(first_pos.Y - last_pos.Y) >= len_displacement*cos(max_deviation_angle))  && ( mod(first_pos.X - last_pos.X) <= len_displacement*sin(max_deviation_angle) ) )  //vertical sweep detected
          {

            if(first_pos.Y > last_pos.Y)//its a reverse case here
               {
                  return 3;//bottom to up
               }
               else
               {
                   return 4;//up to bottom
               }
          }


     }




  return -1;//no valid sweep detected
}


}gesture_ob;

XKeyEvent createKeyEvent(Display *display, Window &win,
                           Window &winRoot, bool press,
                           int keycode, int modifiers)
{
   XKeyEvent event;

   event.display     = display;
   event.window      = win;
   event.root        = winRoot;
   event.subwindow   = None;
   event.time        = CurrentTime;
   event.x           = 1;
   event.y           = 1;
   event.x_root      = 1;
   event.y_root      = 1;
   event.same_screen = False;
   event.keycode     = XKeysymToKeycode(display, keycode);
   event.state       = modifiers;

   if(press)
      event.type = KeyPress;
   else
      event.type = KeyRelease;

   return event;
}



int erode_size,dilate_size,erode_struct,dilate_struct,eli1,eli2;



Mat frame,target_frame,hsv_frame,struct_element,t1,t2,modified_frame,shape_frame,temp,face_cover_frame;


int value = 1;
int size_matrix;

int main()
{int x,y,c,delay_value,k,i;
int cnt=1;

// Obtain the X11 display.
   Display *display = XOpenDisplay(0);
   if(display == NULL)
      return -1;

// Get the root window for the current display.
   Window winRoot = XDefaultRootWindow(display);



// Find the window which has the current keyboard focus.
   Window winFocus;
   int    revert;

  XKeyEvent event;


vector<vector<Point> > contours;
vector<Vec4i> hierarchy;
vector<vector<Point> > result;
previous.X = -1;
previous.Y = -1;

VideoCapture cap(0);
if(cap.isOpened()==0)
{cout<<"error!! \ncan't capture video!!!";
return -1;
}

int in_min_Hue = 0;
int in_max_Hue = 179;

int in_min_Saturation = 0;
int in_max_Saturation = 255;

int in_min_Value = 0;
int in_max_Value = 255;





namedWindow("webcamera",0);
namedWindow("target frame",0);
namedWindow("modified frame",0);
namedWindow("shape frame");



//for median blur
createTrackbar("size of matrix for median blur","target frame",&value,max_size);

//for detecting object
createTrackbar("Min Hue","target frame",&in_min_Hue,val_max_Hue);
createTrackbar("Max Hue","target frame",&in_max_Hue,val_max_Hue);

createTrackbar("Min Saturation","target frame",&in_min_Saturation,val_max_Saturation);
createTrackbar("Max Saturation","target frame",&in_max_Saturation,val_max_Saturation);

createTrackbar("Min Value","target frame",&in_min_Value,val_max_Value);
createTrackbar("Max Value","target frame",&in_max_Value,val_max_Value);


//erosion track bar
createTrackbar("erosion: element\n0-Rect\n1-cross\n2-ellipse","target frame",&erode_struct,2);
createTrackbar("erosion: size","target frame",&erode_size,21);

//dilation track bar
createTrackbar("dilation:element\n0-Rect\n1-cross\n2-ellipse","target frame",&dilate_struct,2);
createTrackbar("dilation:size","target frame",&dilate_size,21);



cap.read(temp);//we are reading a temporary frame from the video stream to detect its size and type
shape_frame = Mat::zeros(temp.size(),temp.type());
Mat contour_detect = shape_frame;

while(1)
{
 bool frame_success = cap.read(frame);
 if(frame_success == 0)
 {
  cout<<"error!! \n can't read frame from video stream";
 }

switch(erode_struct)
{
  case 0: eli1=MORPH_RECT;
           break;
  case 1:  eli1=MORPH_CROSS;
           break;

  case 2:  eli1=MORPH_ELLIPSE;
           break;
}

switch(dilate_struct)
{
  case 0: eli2=MORPH_RECT;
           break;
  case 1:  eli2=MORPH_CROSS;
           break;

  case 2:  eli2=MORPH_ELLIPSE;
           break;

}

size_matrix= 2* value + 1;//the size of the matrix should be odd
medianBlur(frame,target_frame,size_matrix);
medianBlur(frame,frame,size_matrix);



cvtColor(frame,hsv_frame,COLOR_BGR2HSV);


inRange(hsv_frame,Scalar(in_min_Hue,in_min_Saturation,in_min_Value),Scalar(in_max_Hue,in_max_Saturation,in_max_Value),target_frame);


t1 = getStructuringElement(eli1,Size(2*erode_size + 1,2*erode_size + 1 ));

t2 = getStructuringElement(eli2,Size(2*dilate_size + 1,2*dilate_size + 1 ));


//morphological closing
dilate(target_frame,target_frame,t2);
//erode(target_frame,target_frame,t1);

//calculating moments
Moments moments_ob = moments(target_frame);
double area = moments_ob.m00;
double m01 = moments_ob.m01;
double m10 = moments_ob.m10;



present.X = (m10/area);
present.Y = (m01/area);

if(present.X > 0 && present.Y > 0)
{
if(displacement(present,previous) > 14   )
{

 arrowedLine(shape_frame,Point(previous.X,previous.Y),Point(present.X,present.Y),Scalar(255,255,255),8);


 if(idle_ob.get_status()== 1)//if idle
 {
  first=previous;//storing the initial point of the line
  idle_ob.change_status();
  idle_ob.inc_frame_cnt();
 }
else//not idle
{

idle_ob.reset_frame_cnt();

}

  previous.X = present.X;
  previous.Y = present.Y;


}
else
{
   if(idle_ob.get_status() == 0)//if not idle
   {


     if(idle_ob.get_frame_cnt() < idle_ob.get_max_frame_cnt())
      {
         idle_ob.inc_frame_cnt();

      }
      else//frame_cnt = max_frame
      {

       last=present;
       i=0;
       /*detecting sweep gestures*/
       //this logic has been designed keeping in mind just the sweep gesture
       //requires modifiacation to include other gestures

       switch(gesture_ob.detect_sweep(first,last))
       {
          case 1://right to left sweep
                 cout<<"rt 2 left:  "<<KEYCODE_1<<endl;

                    XGetInputFocus(display, &winFocus, &revert);


                    // Send a fake key press event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, true, KEYCODE_1, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                    // Send a fake key release event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, false, KEYCODE_1, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);
                      XSync(display,true);

                  break;

          case 2://left to right sweep
                  cout<<"left 2 right: "<<KEYCODE_2<<endl;


                    XGetInputFocus(display, &winFocus, &revert);

                    // Send a fake key press event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, true, KEYCODE_2, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                    // Send a fake key release event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, false, KEYCODE_2, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);
                      XSync(display,true);
                    break;

          case 3://bottom to up sweep
                 cout<<"bottom 2 up\n";


                    XGetInputFocus(display, &winFocus, &revert);

                    XSync(display,true);
                    // Send a fake key press event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, true, KEYCODE_3, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                   // Send a fake key release event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, false, KEYCODE_3, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                    XSync(display,true);


                  break;

          case 4://up to bottom sweep
                 cout<<"up 2 bottom\n";

                    XGetInputFocus(display, &winFocus, &revert);

                    XSync(display,true);
                    // Send a fake key press event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, true, KEYCODE_4, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                    // Send a fake key release event to the window.
                    event = createKeyEvent(display, winFocus, winRoot, false, KEYCODE_4, 0);
                    XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

                        XSync(display,true);



                  break;

             default:cout<<"no gesture\n";


       }


      idle_ob.change_status();
       idle_ob.reset_frame_cnt();
       shape_frame=Mat::zeros(temp.size(),temp.type());//clear shape frame
      }

   }


}

}


imshow("shape frame",shape_frame);
imshow("webcamera",frame);
//imshow("hsv frame",hsv_frame);
imshow("target frame",target_frame);
//imshow("erosion frame",erode_frame);
//imshow("dilation frame",dilate_frame);
imshow("modified frame",target_frame);

// cnt=(cnt+1)%10;
 waitKey( 5);

}

return 0;

}

