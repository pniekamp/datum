//
// Datum - asset embed
//

//
// Copyright (c) 2016 Peter Niekamp
//

#include <QGuiApplication>
#include <QImage>
#include <QFileInfo>
#include <iostream>
#include <leap.h>

using namespace std;
using namespace leap;


void embed_image(const char *path)
{
  QImage image = QImage(path).convertToFormat(QImage::Format_ARGB32);

  cout << "  // " << QFileInfo(path).fileName().toStdString() << endl;

  cout << "  struct" << endl;
  cout << "  {" << endl;
  cout << "    int width = " << image.width() << ";" << endl;
  cout << "    int height = " << image.height() << ";" << endl;

  cout << "    uint32_t data[" << image.width() * image.height() << "] =" << endl;
  cout << "    {" << endl;

  for(int y = 0; y < image.height(); ++y)
  {
    cout << "      ";

    for(int x = 0; x < image.width(); ++x)
    {
      cout << "0x" << hex << image.pixel(x, y) << dec << ", ";
    }

    cout << endl;
  }

  cout << "    };" << endl;

  cout << endl;
  cout << "  } " << QFileInfo(path).baseName().toStdString() << ";" << endl;

  cout << endl;
}


int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);

  cout << "//" << endl;
  cout << "// Embeded Assets" << endl;
  cout << "//" << endl;
  cout << endl;
  cout << "namespace embeded" << endl;
  cout << "{" << endl;

  try
  { 
    for(int i = 1; i < argc; ++i)
    {
      embed_image(argv[i]);
    }
  }
  catch(std::exception &e)
  {
    cerr << "Critical Error: " << e.what() << endl;
  }

  cout << "}" << endl;
}
