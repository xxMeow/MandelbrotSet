#pragma comment(lib, "Gdiplus.lib")

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h> // Get cwd
#include <gdiplus.h>
#include "mpi.h"

using namespace Gdiplus;

#define EDGE_PIXEL_NUM 400 // Aka. display_width
#define COLOR_LEVEL_MAX 255
#define BMP_PATH L"DemoMPI.bmp"

struct Complex { // Define complex number with some operations
    float real;
    float imag;

    Complex() : real(0.0), imag(0.0) {}
    Complex(float r, float i) : real(r), imag(i) {}

    Complex operator+(const Complex& other) { // complex + complex
        return Complex(this->real + other.real, this->imag + other.imag);
    }
    Complex operator-(const Complex& other) { // complex - complex
        return Complex(this->real - other.real, this->imag - other.imag);
    }
    Complex operator*(const Complex& other) { // complex * complex
        Complex result;
        result.real = this->real * other.real - this->imag * other.imag;
        result.imag = this->imag * other.real + this->real * other.imag;
        return result;
    }

    Complex operator+(const float& num) { // complex + float
        return Complex(this->real + num, this->imag);
    }
    Complex operator-(const float& num) { // complex + float
        return Complex(this->real - num, this->imag);
    }
    Complex operator*(const float& num) { // complex * float
        return Complex(this->real * num, this->imag * num);
    }
    Complex operator/(const float& num) { // complex / float
        return Complex(this->real / num, this->imag / num);
    }
    float lenSq() { // Calculate the squared length of complex
        return (this->real * this->real + this->imag * this->imag);
    }
};
struct ComplexPlane {
    Complex lu; // left up
    Complex ru; // right up
    Complex lb; // left bottom
    Complex rb; // right bottom

    ComplexPlane(Complex luCoord, Complex size) {
        this->lu = luCoord;
        this->ru = this->lu + size.real;
        this->lb = this->lu + size.imag;
        this->rb = this->ru + size;
    }
};

enum Tag {
    TAG_INFO,
    TAG_DATA,
    TAG_STOP
};

/* Function Declarition */
int calculatePixel(Complex startPoint, float scaleW, int indexW, float scaleH, int indexH);
int saveAsBmpFile(int w, int h, BYTE* pixelData); // Save pixelData as BMP to BMP_PATH


int main(int argc, char* argv[])
{
    // Complex plane
    Complex complexPlaneLU(-2.0, -2.0);
    Complex complexPlaneSize(4.0, 4.0);
    ComplexPlane complexPlane(complexPlaneLU, complexPlaneSize);

    // Mapping scales
    float scaleW = complexPlaneSize.real / EDGE_PIXEL_NUM;
    float scaleH = complexPlaneSize.imag / EDGE_PIXEL_NUM;

    LARGE_INTEGER timeFreq, timeStart, timeEnd;
    QueryPerformanceFrequency(&timeFreq);
    QueryPerformanceCounter(&timeStart);

    // Static
    /* BEGIN --------------------------------------------------------------- */

    MPI_Init(&argc, &argv);

    int procNum;
    MPI_Comm_size(MPI_COMM_WORLD, &procNum); // Get # of process
    if (procNum <= 1) {
        printf("ERROR: Number of process should be >= 2. Since there must be 1 slave at least.\n");
        MPI_Finalize();
        exit(-1);
    }
    int myRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank); // Get self rank

    MPI_Status status;

    if (myRank == 0) { // Master

        BYTE* bmpData = new BYTE[EDGE_PIXEL_NUM * EDGE_PIXEL_NUM];

        // Buffer preparation
        int sendBuffer[2]; // [startColNo, endColNo]
        int recvBuffer[3]; // [coordX, coordY, color]

        int startColNo = 0;
        int taskSize = EDGE_PIXEL_NUM - 1;
        if (procNum > 1) {
            taskSize = (EDGE_PIXEL_NUM + procNum - 2) / (procNum - 1); // So the remainder can be dropped safely
        }
        int endColNo = startColNo + taskSize;

        // Task assignment: Each PE will be assigned with [startColNo, endColNo) columns
        for (int i = 1; i < procNum; i ++) { // Assign task for each slave
            sendBuffer[0] = startColNo;
            sendBuffer[1] = endColNo >= EDGE_PIXEL_NUM ? EDGE_PIXEL_NUM : endColNo;
            MPI_Send(sendBuffer, 2, MPI_INT, i, TAG_INFO, MPI_COMM_WORLD);

            startColNo = endColNo;
            endColNo = startColNo + taskSize;
        }

        // Result collection
        for (int i = 0; i < EDGE_PIXEL_NUM * EDGE_PIXEL_NUM; i ++) {
            MPI_Recv(recvBuffer, 3, MPI_INT, MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &status);
            bmpData[recvBuffer[0] * EDGE_PIXEL_NUM + recvBuffer[1]] = recvBuffer[2]; // Fill the pixel data
        }

        // BMP generation & Memory Releas
        saveAsBmpFile(EDGE_PIXEL_NUM, EDGE_PIXEL_NUM, bmpData);
        delete[]bmpData;

        QueryPerformanceCounter(&timeEnd);
        double timeDiff = (double)(timeEnd.QuadPart - timeStart.QuadPart) / (double)timeFreq.QuadPart;
        printf("Static[%d Slave(s)]: Run for %fs.\n", procNum - 1, timeDiff);

    } else { // Slaves

        // Buffer preparation
        int recvBuffer[2]; // [startColNo, endColNo]
        int sendBuffer[3]; // [coordX, coordY, color]

        // Task acception
        MPI_Recv(recvBuffer, 2, MPI_INT, 0, TAG_INFO, MPI_COMM_WORLD, &status);

        // Task execution
        for (int i = 0; i < EDGE_PIXEL_NUM; i ++) {
            for (int j = recvBuffer[0]; j < recvBuffer[1]; j ++) {
                sendBuffer[0] = i;
                sendBuffer[1] = j;
                sendBuffer[2] = calculatePixel(complexPlane.lu, scaleW, i, scaleH, j);
                MPI_Send(sendBuffer, 3, MPI_INT, 0, TAG_DATA, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Finalize();

    /* END ----------------------------------------------------------------- */

    return 0;
}

int calculatePixel(Complex planeOrigin, float scaleW, int indexW, float scaleH, int indexH) {
    Complex offset(scaleW * indexW, scaleH * indexH);
    Complex c = planeOrigin + offset; // Mapping

    int count = 0;
    Complex z(0.0, 0.0);
    do {
        z = z * z + c;
        count ++;
    } while (z.lenSq() < 4.0 && count < COLOR_LEVEL_MAX);

    return count;
}

/* Generate grayscale BMP file from pixel data */

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0; // number of image encoders
    UINT  size = 0; // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1; // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1; // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j; // Success
        }
    }

    free(pImageCodecInfo);
    return -1; // Failure
}

int saveAsBmpFile(int w, int h, BYTE* pixelData) {
    // Initialize GDI+.
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    int paletteCount = 256;
    ColorPalette* palette = (ColorPalette*)malloc(2 * sizeof(UINT) + paletteCount * sizeof(ARGB));
    palette->Flags = PaletteFlags::PaletteFlagsGrayScale;
    palette->Count = paletteCount;
    for (int i = 0; i < 256; i ++) {
        palette->Entries[i] = Color::MakeARGB(1, i, i, i);
    }

    CLSID   encoderClsid;
    Status  stat;
    Bitmap* bitmap = new Bitmap(w, h, w, PixelFormat8bppIndexed, pixelData);
    bitmap->SetPalette(palette);

    // Get the CLSID of the PNG encoder.
    GetEncoderClsid(L"image/bmp", &encoderClsid);

    stat = bitmap->Save(L"Mandelbrot.bmp", &encoderClsid, NULL);
    if (stat == Ok) {
        char cwd[1024];
        printf("BMP was generated at: %s\\Mandelbrot.bmp\n", _getcwd(cwd, 1023));
    } else {
        printf("Failure: stat = %d\n", stat);
    }

    delete bitmap;
    GdiplusShutdown(gdiplusToken);

    free(palette);

    return 0;
}
