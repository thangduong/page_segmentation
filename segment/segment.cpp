#include <Windows.h>
#include <chrono>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include "segment.h"
using namespace cv;
using namespace std;
void min_pix2(Mat& in, Mat& out, int threshold = 200) {
    // input is RGB
    // output is grayscale min(R,G,B)
    auto indata = (const uint8_t*)in.data;
    auto outdata = (uint8_t*)out.data;
    int h = in.rows;
    int w = in.cols;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            outdata[x] = indata[2 * x+1];
            if (outdata[x] > threshold)
                outdata[x] = 0;
            else
                outdata[x] = 255;
        }
        indata += w * 2;
        outdata += w;
    }
}
void min_pix(Mat& in, Mat& out, int threshold = 200) {
    // input is RGB
    // output is grayscale min(R,G,B)
    auto indata = (const uint8_t*)in.data;
    auto outdata = (uint8_t*)out.data;
    int h = in.rows;
    int w = in.cols;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            outdata[x] = min(min(indata[3 * x], indata[3 * x + 1]), indata[3 * x + 2]);
            if (outdata[x] > threshold)
                outdata[x] = 0;
            else
                outdata[x] = 255;
        }
        indata += w * 3;
        outdata += w;
    }
}

//void compute_on_density()

list<pair<int,int>> hscan(Mat& in, int x0 = 0, int y0 = 0, int xf = -1, int yf = -1) {
    list<pair<int, int>> result;

    pair<int, int> cur;

    if (xf <= 0) xf = in.cols;
    if (yf <= 0) yf = in.rows;

    // -1 = blank slate
    // 0 = in blank
    // 1 = in text
    //
    int state = -1;
    auto indata = (const uint8_t*)in.data;
    for (int x = x0; x < xf; x++) {
        int num_on_count = 0;
        for (int y = y0; y < yf; y++) {
            num_on_count += (indata[y*in.cols] ? 1 : 0);
        }
        indata +=1;

        bool blank_line = num_on_count < 1;
        if (state == -1) {
            if (blank_line) {
                cur.first = x;
                state = 0;
            }
            else
                state = 1;
        }
        else if (state == 1) {
            if (blank_line) {
                cur.first = x;
                state = 0;
            }
        }
        else if (state == 0) {
            if (blank_line) {
                cur.second = x;
            }
            else {
                state = 1;
                result.push_back(cur);
            }
        }
    }
    if (state == 0) {
        cur.second += 1;
        result.push_back(cur);
    }
    return result;
}

list<pair<int, int>> vscan(Mat& in, int x0 = 0, int y0 = 0, int xf = -1, int yf = -1) {
    list<pair<int, int>> result;

    pair<int, int> cur;

    if (xf <= 0) xf = in.cols;
    if (yf <= 0) yf = in.rows;

    // -1 = blank slate
    // 0 = in blank
    // 1 = in text
    //
    int state = -1;

    auto indata = (const uint8_t*)in.data + y0 * in.cols;
//    auto lastline = indata;

    for (int y = y0; y < yf; y++) {
        int num_on_count = 0;
//        auto nextline = indata;
        bool blank_line = true;
        for (int x = x0; x < xf; x++) {
            if (indata[x])// && lastline[x] && nextline[x])
            {
                blank_line = false;
                break;
            }
            //num_on_count += ((indata[x]) ? 1 : 0);
        }
        //        bool blank_line = num_on_count < 1;
//        lastline = indata;
        indata += in.cols;

        if (state == -1) {
            if (blank_line) {
                cur.first = y;
                cur.second = y;
                state = 0;
            }
            else
                state = 1;
        }
        else if (state == 1) {
            if (blank_line) {
                cur.first = y;
                cur.second = y;
                state = 0;
            }
        }
        else if (state == 0) {
            if (blank_line) {
                cur.second = y;
            }
            else {
                state = 1;
                result.push_back(cur);
            }
        }
    }
    if (state == 0) {
        cur.second += 1;
        result.push_back(cur);
    }
    return result;
}

void filter_breaks(list<pair<int, int>>& breaks, int min_size) {
    auto bi = breaks.begin();
    while (bi != breaks.end()) {
        if (bi->second - bi->first <= min_size) {
            auto bj = bi;
            bj++;
            breaks.erase(bi);
            bi = bj;
        }
        else
            bi++;
    }
}
Rect crop_rect(Mat& image, Rect r) {
    auto data = (uint8_t*)image.data;
    data += r.y * image.cols;
    int minx = r.x + r.width;
    int miny = r.y + r.height;
    int maxy = r.y;
    int maxx = r.x;
    for (int y = r.y; y < r.y+r.height; y++) {
        for (int x = r.x; x < r.x+r.width; x++) {
            if (data[x]) {
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                if (y > maxy) maxy = y;
            }
        }
        data += image.cols;
    }
    return Rect(minx, miny, maxx - minx, maxy - miny);
}

list<Rect> vprocess(const list<Rect>& inlist, Mat& in, Mat& green, int min_break_size, bool merge_by_paragraph = false, int paragraph_overide_threshold = 10, int justify_threshold=10, bool draw_red=false) {
    list<Rect> result;
    for (auto i = inlist.begin(); i != inlist.end(); i++) {
        list<pair<int, int>> hbreaks = vscan(in, i->x, i->y, i->x + i->width, i->y + i->height);
        filter_breaks(hbreaks, min_break_size);
        auto hi = hbreaks.begin();

        hbreaks.push_back(pair<int, int>(i->y+i->height, i->y + i->height));
        pair<int, int> last_pair = pair<int, int>(i->y, i->y);
        Rect r;

        int last_type = -1;

        // result in current rect
        list<Rect> current_result;
        bool new_paragraph = true;
        while (hi != hbreaks.end()) {
            bool draw_green = true;
            r = Rect(i->x, last_pair.second, i->width, hi->first - last_pair.second);
            if (r.height > 0) {
                if (last_pair.second - last_pair.first > paragraph_overide_threshold)
                    new_paragraph = true;
                if (merge_by_paragraph) {
//                    cout << "VALUE = " << hi->second - hi->first << endl;
                    Rect cr = crop_rect(in, r);
                    int dx0 = (cr.x - r.x);
                    int dxf = (r.x + r.width - cr.x - cr.width);
//                    cout << dx0 << " " << dxf << endl;

                    if (new_paragraph)
                        current_result.push_back(r);
                    else {
                        draw_green = false;
                        current_result.back().height = r.y + r.height - current_result.back().y;
                    }

                    int djustify = dxf - dx0;
                    if (djustify > justify_threshold) {
                        // left justified EOP
                        new_paragraph = true;
                    }
                    //else if (djustify < -justify_threshold) {
                        // right justified EOP
                    //    new_paragraph = true;
                    //}
                    else {
                        // not EOP or center justified (also not EOP)
                        //
                        // this is not line break, so we just merge 
                        // current rect with last ect
                        new_paragraph = false;
                    }
                }
                else {
                    current_result.push_back(r);
                }
                if ((!draw_green) && draw_red){
//                    cout << "RED : " << hi->second - hi->first << endl;
                    rectangle(green, Rect(i->x, last_pair.first, i->width, last_pair.second - last_pair.first), Scalar(0, 0, 0xff), -1, 8);
                }
            }
            if (draw_green)
                rectangle(green, Rect(i->x, last_pair.first, i->width, last_pair.second - last_pair.first), Scalar(0, 0xff, 0), -1, 8);
            last_pair = *hi;
            hi++;
        }

        result.splice(result.end(), current_result);
    }
    return result;
}


list<Rect> hprocess(const list<Rect>& inlist, Mat& in, Mat& green, int min_break_size) {
    list<Rect> result;
    for (auto i = inlist.begin(); i != inlist.end(); i++) {
        list<pair<int, int>> vbreaks = hscan(in, i->x, i->y, i->x + i->width, i->y + i->height);
        filter_breaks(vbreaks, min_break_size);

        auto hi = vbreaks.begin();
        while (hi != vbreaks.end()) {
            auto hj = hi;
            hi++;
            rectangle(green, Rect(hj->first, i->y, hj->second - hj->first, i->height), Scalar(0, 0xff, 0), -1, 8);
            if (hi != vbreaks.end()) {
                Rect r = Rect(hj->second, i->y, hi->first - hj->second, i->height);;
                result.push_back(r);
            }
        }

    }
    return result;
}

int main(int argc, char** argv)
{
    auto begin = chrono::high_resolution_clock::now();
//    string input_file = "C:\\section_segmentation\\data\\dlpaper.jpeg";
    string input_file = "C:\\section_segmentation\\data\\evorus.png";
    string output_dir = "C:\\section_segmentation\\data\\";
    if ((argc == 2) && (string(argv[1]) == "-h")) {
        cout << "Usage: " << argv[0] << " <input-file> [<output-dir>]" << endl;
        cout << "If [<output-dir>] is left empty, output is the current directory." << endl;
    }

    if (argc >= 2)
        input_file = argv[1];
    if (argc >= 3)
        output_dir = argv[2];
    cout << "Loading: " << input_file << endl;
    cout << "Output dir: " << output_dir << endl;

    Mat image;
    image = imread(input_file, IMREAD_COLOR);   // Read the file
    Mat bgra[3];   //destination array
    split(image, bgra);//split source  


    if (!image.data)                              // Check for invalid input
    {
        cout << "Could not open or find the image" << std::endl;
        return -1;
    }
    Mat minrgb, green;
    image.copyTo(green);
    minrgb.create(image.rows, image.cols, CV_8UC1);

    min_pix(image, minrgb);

    int dilation_size = 4;
    Mat element = getStructuringElement(MORPH_ELLIPSE,
        Size(2 * MORPH_ELLIPSE + 1, 2 * dilation_size + 1),
        Point(dilation_size, dilation_size));
    
    for (int i = 0; i < 0; i++) 
    {
        dilate(minrgb, minrgb, element);
        erode(minrgb, minrgb, element);
    }
    Rect entire_image = Rect(Point(0, 0), Point(image.cols, image.rows));
    list<Rect> cur_rects = { entire_image };
    cur_rects = vprocess(cur_rects, minrgb, green, 15);
    cur_rects = hprocess(cur_rects, minrgb, green, 15);
    cur_rects = vprocess(cur_rects, minrgb, green, 0, true, 8, 5, false);

    string green_file = input_file.substr(0, input_file.length() - 4) + ".green.png";
    imwrite(green_file, green);
    int region_index = 0;
    for (Rect ri : cur_rects) {
        char filename[128];
        sprintf(filename, "%s%04d.jpg", output_dir.c_str(), region_index++);
        Mat imgRgn = image(ri);
        imwrite(filename, imgRgn);
    }


    auto end = std::chrono::high_resolution_clock::now();
//    cout << "Generated " << region_index << " regions" << endl;
    cout << "Total run time: " << chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()/ 1000000 << "ms" << std::endl;
    
    namedWindow("Display window", WINDOW_AUTOSIZE);// Create a window for display.
    imshow("Display window", bgra[2]);                   // Show our image inside it.
                                                        
    namedWindow("Display window2", WINDOW_AUTOSIZE);// Create a window for display.
    imshow("Display window2", bgra[0]);                   // Show our image inside it.

    waitKey(0);                                          // Wait for a keystroke in the window
  
    return 0;
}