#include <Windows.h>
#include <chrono>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include <gflags/gflags.h>
#include "segment.h"


DEFINE_string(input_file, "C:\\section_segmentation\\data\\evorus-0007.png", "Image input file");
DEFINE_string(output_dir, "C:\\section_segmentation\\data\\", "Where to store output files");
DEFINE_int32(erode_dilate_count, 0, "# of times to erode and dilate");
DEFINE_int32(dilate_size, 4, "Size of dilation kernel");
DEFINE_int32(binarize_threshold, 200, "Threshold used to binarize the image");
DEFINE_int32(paragraph_overide_threshold, 8, "Break larger than this don't get merged by paragraph");
DEFINE_int32(justify_threshold, 5, "Number of pixels misaligned to count as last sentence");
DEFINE_int32(vp0_min_break_size, 15, "First vertical pass minimum break size.  vp0, hp0, vp1.");
DEFINE_int32(hp0_min_break_size, 15, "First horizontal pass minimum break size.  vp0, hp0, vp1.");
DEFINE_int32(vp1_min_break_size, 15, "Second vertical pass minimum break size.  vp0, hp0, vp1.");
DEFINE_bool(merge_by_paragraph, true, "Merge nearby lines and detect when a paragraph ends");
DEFINE_bool(show_merged, false, "Show where merging happened via merge_by_paragraph on the green output");
DEFINE_bool(verbose, true, "Output verbose logging");
DEFINE_bool(debug, false, "Show some output images");

using namespace cv;
using namespace std;

void binarize(Mat& in, Mat& out, int threshold = 200) {
    // input is RGB
    // output is grayscale min(R,G,B)
    // anything above threshold is set to 0
    // anything below threshold is set to 255
    auto indata = (uint8_t*)in.data;
    auto outdata = (uint8_t*)out.data;
    int h = in.rows;
    int w = in.cols;
    if (in.type() == CV_8UC3) {
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
    else if (in.type() == CV_8UC4) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // NOTE: RGB probably should be BGR.
                int r = indata[4 * x + 0];
                int g = indata[4 * x + 1];
                int b = indata[4 * x + 2];
                int a = indata[4 * x + 3];
                float alpha = ((float)a) / 255.0f;
                r = r*alpha + 255 * (1 - alpha);
                g = g*alpha + 255 * (1 - alpha);
                b = b*alpha + 255 * (1 - alpha);
                outdata[x] = min(min(r,g),b);
                if (outdata[x] > threshold)
                    outdata[x] = 0;
                else
                    outdata[x] = 255;
                indata[4 * x + 0] = r;
                indata[4 * x + 1] = g;
                indata[4 * x + 2] = b;
                indata[4 * x + 3] = 0xff;
            }
            indata += w * 4;
            outdata += w;
        }
        cvtColor(in, in, CV_BGRA2BGR);
    }
}

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
    // filter out breaks that are < min_size
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
    ///
    // Find the smallest rect around all the data that is entirely inside
    // rect.
    ///
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

list<Rect> vprocess(const list<Rect>& inlist, Mat& in, Mat& green, int min_break_size, 
    bool merge_by_paragraph = false,
    int paragraph_overide_threshold = 10, 
    int justify_threshold = 10, 
    bool draw_red = false) {
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
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto begin = chrono::high_resolution_clock::now();
    string input_file = FLAGS_input_file;
    string output_dir = FLAGS_output_dir;

    if (argc >= 2)
        input_file = argv[1];
    if (argc >= 3)
        output_dir = argv[2];
    cout << "Loading: " << input_file << endl;
    cout << "Output dir: " << output_dir << endl;

    Mat image;
    image = imread(input_file, IMREAD_UNCHANGED);   // Read the file


    if (!image.data)                              // Check for invalid input
    {
        cout << "Could not open or find the image" << std::endl;
        return -1;
    }
    Mat minrgb;
    Mat& green = image;
    minrgb.create(image.rows, image.cols, CV_8UC1);

    binarize(image, minrgb, FLAGS_binarize_threshold);


    // dilate and erode to merge nearby componenta
    int dilation_size = FLAGS_dilate_size;
    Mat element = getStructuringElement(MORPH_ELLIPSE,
        Size(2 * MORPH_ELLIPSE + 1, 2 * dilation_size + 1),
        Point(dilation_size, dilation_size));
    
    for (int i = 0; i < FLAGS_erode_dilate_count; i++)
    {
        dilate(minrgb, minrgb, element);
        erode(minrgb, minrgb, element);
    }

    Rect entire_image = Rect(Point(0, 0), Point(image.cols, image.rows));
    list<Rect> cur_rects = { entire_image };

    // vertical scan, horizontal scan, then vertical scan again
    cur_rects = vprocess(cur_rects, minrgb, green, FLAGS_vp0_min_break_size);
    cur_rects = hprocess(cur_rects, minrgb, green, FLAGS_hp0_min_break_size);
    cur_rects = vprocess(cur_rects, minrgb, green, FLAGS_vp1_min_break_size, FLAGS_merge_by_paragraph, FLAGS_paragraph_overide_threshold, FLAGS_justify_threshold, FLAGS_show_merged);


    // save output
    string green_file = input_file.substr(0, input_file.length() - 4) + ".green.png";
    if (FLAGS_verbose)
        cout << "Writing: " << green_file << endl;
    imwrite(green_file, green);
    int region_index = 0;
    ofstream coordinates_file(output_dir + "coordinates.txt");
    for (Rect ri : cur_rects) {
        char filename[128];
        coordinates_file << region_index << " " << ri.x << " " << ri.y << " " << ri.width << " " << ri.height << endl;
        sprintf(filename, "%s%04d.png", output_dir.c_str(), region_index++);
        Mat imgRgn = image(ri);
        if (FLAGS_verbose)
            cout << "Writing: " << filename << endl;
        imwrite(filename, imgRgn);
    }
    coordinates_file.close();


    auto end = std::chrono::high_resolution_clock::now();

    if (FLAGS_verbose) {
        cout << "Generated " << region_index << " regions" << endl;
        cout << "Total run time: " << chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1000000 << "ms" << std::endl;
    }
    
    if (FLAGS_debug) {
        namedWindow("Original", WINDOW_AUTOSIZE);
        imshow("Original", image);

        namedWindow("MinRGB", WINDOW_AUTOSIZE);
        imshow("MinRGB", minrgb);

        namedWindow("Green", WINDOW_AUTOSIZE);
        imshow("Green", green);
        cout << "Press any key to exit..." << endl;
        waitKey(0);                                    
    }
    return 0;
}