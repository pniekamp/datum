//
// Datum - debug log viewer
//

//
// Copyright (c) 2017 Peter Niekamp
//

#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QAbstractScrollArea>
#include <QWheelEvent>
#include <QScrollBar>
#include <QMenuBar>
#include <QFileDialog>
#include <QPainter>
#include <QSettings>
#include "datum/math.h"
#include <leap.h>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <cassert>

#include <QDebug>

using namespace std;
using namespace lml;
using namespace leap;

//
// Timing
//

struct DebugInfoBlock
{
  const char *name;
  const char *filename;
  int linenumber;
  Color3 color;
};

struct DebugLogEntry
{
  enum EntryType
  {
    Empty,
    FrameMarker,
    EnterBlock,
    ExitBlock,
    GpuSubmit,
    GpuBlock,

    RenderLump,
    RenderStorage,
    ResourceSlot,
    ResourceBuffer,
    EntitySlot,

    HitCount
  };

  EntryType type;
  std::thread::id thread;
  unsigned long long timestamp;

  union
  {
    size_t hitcount;

    struct
    {
      uint32_t resourceused;
      uint32_t resourcecapacity;
    };

    DebugInfoBlock const *info;
  };
};

//
// Log Dump
//

#pragma pack(push, 1)

struct DebugLogHeader
{
  uint32_t magic;
  double clockfrequency;
};

struct DebugLogChunk
{
  uint32_t length;
  uint32_t type;
};

struct DebugLogInfoChunk // type = 1
{
  DebugInfoBlock const *id;

  char name[256];
  char filename[512];
  int linenumber;
  Color3 color;
};

struct DebugLogEntryChunk // type = 2
{
  uint32_t entrycount;
  DebugLogEntry entries[1];
};

#pragma pack(pop)

namespace
{

}


//-------------------------- DebugView --------------------------------------
//---------------------------------------------------------------------------

class DebugView : public QAbstractScrollArea
{
  Q_OBJECT

  public:
    DebugView(QWidget *parent = 0);

  public slots:

    void load(unordered_map<DebugInfoBlock const *, DebugLogInfoChunk> const &infos, vector<DebugLogEntry> const &log, double clockfrequency);

  protected:

    void invalidate();

    void wheelEvent(QWheelEvent *event);

    void resizeEvent(QResizeEvent *event);

    void paintEvent(QPaintEvent *event) override;

  private:

    bool m_showframetimes;
    bool m_showframemarkers;

    double m_mintime, m_maxtime;

    vector<double> m_framemarkers;

    int m_width;
    int m_height;
    int m_border;

    double m_scale;
};


///////////////////////// DebugView::Constructor ////////////////////////////
DebugView::DebugView(QWidget *parent)
  : QAbstractScrollArea(parent)
{
  m_scale = 1;
  m_width = 100;
  m_height = 150;
  m_border = 4;

  m_showframetimes = true;
  m_showframemarkers = true;
}


///////////////////////// DebugView::load ///////////////////////////////////
void DebugView::load(unordered_map<DebugInfoBlock const *, DebugLogInfoChunk> const &infos, vector<DebugLogEntry> const &log, double clockfrequency)
{
  m_mintime = numeric_limits<double>::max();
  m_maxtime = numeric_limits<double>::lowest();

  for(auto &entry : log)
  {
    if (entry.type == DebugLogEntry::GpuBlock)
      continue;

    m_mintime = min(m_mintime, entry.timestamp / clockfrequency);
    m_maxtime = max(m_maxtime, entry.timestamp / clockfrequency);
  }

  for(auto &entry : log)
  {
    if (entry.type == DebugLogEntry::FrameMarker)
    {
      m_framemarkers.push_back(entry.timestamp / clockfrequency - m_mintime);
    }
  }

  m_scale = (viewport()->width() - 2*m_border) / (m_maxtime - m_mintime);

  invalidate();
}


///////////////////////// DebugView::invalidate /////////////////////////////
void DebugView::invalidate()
{
  m_width = m_scale * (m_maxtime - m_mintime);

  horizontalScrollBar()->setRange(0, m_width + 2*m_border - viewport()->width());
  horizontalScrollBar()->setPageStep(viewport()->width());

  verticalScrollBar()->setRange(0, m_height + 2*m_border - viewport()->height());
  verticalScrollBar()->setPageStep(viewport()->height());

  viewport()->update();
}


///////////////////////// DebugView::wheelEvent /////////////////////////////
void DebugView::wheelEvent(QWheelEvent *event)
{
  auto focus = event->pos();
  auto scale = m_scale * (1 + 0.001*event->angleDelta().y());

  auto locus = QPointF(horizontalScrollBar()->value() + focus.x(), verticalScrollBar()->value() + focus.y()) / m_scale;

  m_scale = max(scale, (viewport()->width() - 2*m_border) / (m_maxtime - m_mintime));

  invalidate();

  horizontalScrollBar()->setValue(locus.x()*m_scale - focus.x());
  verticalScrollBar()->setValue(locus.y()*m_scale - focus.y());
}


///////////////////////// DebugView::resizeEvent ////////////////////////////
void DebugView::resizeEvent(QResizeEvent *event)
{
  invalidate();
}


///////////////////////// DebugView::paintEvent /////////////////////////////
void DebugView::paintEvent(QPaintEvent *event)
{
  QPainter painter(viewport());

  int x = -horizontalScrollBar()->value();
  int y = -verticalScrollBar()->value();

  int cursor = m_border;

  if (m_showframemarkers)
  {
    for(auto &marker : m_framemarkers)
    {
      painter.setPen(Qt::lightGray);
      painter.drawLine(x+m_border + marker*m_scale, 0, x+m_border + marker*m_scale, viewport()->height());
    }
  }

  if (m_showframetimes)
  {
    const int FrameTimesHeight = 50;

    for(size_t i = 0; i < max(m_framemarkers.size(), size_t(1)) - 1; ++i)
    {
      double x0 = m_framemarkers[i];
      double x1 = m_framemarkers[i+1];
      double y0 = clamp((x1 - x0) / (1.0/30.0), 0.0, 1.0);

      painter.setPen(Qt::blue);
      painter.drawLine(x+m_border + x0*m_scale, y+cursor + FrameTimesHeight-y0*FrameTimesHeight, x+m_border + x1*m_scale, y+cursor + FrameTimesHeight-y0*FrameTimesHeight);
    }

    cursor += FrameTimesHeight;
  }
}



//-------------------------- DebugViewer ------------------------------------
//---------------------------------------------------------------------------

class DebugViewer : public QMainWindow
{
  Q_OBJECT

  public:
    DebugViewer();
    virtual ~DebugViewer();

  public slots:

    void load(string const &filename);

  protected slots:

    void on_FileLoad_triggered();

  private:

    DebugView *m_debugview;
};


///////////////////////// DebugViewer::Constructor //////////////////////////
DebugViewer::DebugViewer()
{
  auto menubar = new QMenuBar;
  auto filemenu = menubar->addMenu("&File");
  auto fileload = filemenu->addAction("Load...");

  setMenuBar(menubar);

  m_debugview = new DebugView;

  setCentralWidget(m_debugview);

  connect(fileload, &QAction::triggered, this, &DebugViewer::on_FileLoad_triggered);

  QSettings settings;
  move(settings.value("mainwindow/pos", pos()).toPoint());
  resize(settings.value("mainwindow/size", size()).toSize());
  restoreState(settings.value("mainwindow/state", QByteArray()).toByteArray());
}


///////////////////////// DebugViewer::Destructor ///////////////////////////
DebugViewer::~DebugViewer()
{
  QSettings settings;
  settings.setValue("mainwindow/pos", pos());
  settings.setValue("mainwindow/size", size());
  settings.setValue("mainwindow/state", saveState());
}


///////////////////////// DebugViewer::load /////////////////////////////////
void DebugViewer::load(string const &filename)
{
  ifstream fin(filename, ios_base::binary);

  if (!fin)
    throw runtime_error("Error opening file");

  DebugLogHeader header;

  fin.read((char*)&header, sizeof(header));

  if (header.magic != 0x44544d44)
    throw runtime_error("Invalid File Header");

  vector<DebugLogEntry> log;
  unordered_map<DebugInfoBlock const *,DebugLogInfoChunk> infos;

  while (fin)
  {
    DebugLogChunk chunk;
    fin.read((char*)&chunk, sizeof(chunk));

    switch(chunk.type)
    {
      case 1:
        {
          DebugLogInfoChunk info;
          fin.read((char*)&info, sizeof(info));

          infos[info.id] = info;
        }
        break;

      case 2:
        {
          uint32_t entrycount;
          fin.read((char*)&entrycount, sizeof(entrycount));

          for(uint32_t i = 0; i < entrycount; ++i)
          {
            DebugLogEntry entry;
            fin.read((char*)&entry, sizeof(entry));

            log.push_back(entry);
          }
        }
        break;

      default:
        fin.ignore(chunk.length);
    }
  }

  m_debugview->load(infos, log, header.clockfrequency);
}


///////////////////////// DebugViewer::on_FileLoad_triggered ////////////////
void DebugViewer::on_FileLoad_triggered()
{
  QString src = QFileDialog::getOpenFileName(this, "Load");

  if (src != "")
  {
    try
    {
      load(src.toStdString());
    }
    catch(exception &e)
    {
      QMessageBox::critical(NULL, "Error", QString("Load Error: %1").arg(e.what()));
    }
  }
}


///////////////////////// main //////////////////////////////////////////////
int main(int argc, char **argv)
{
  QApplication app(argc, argv);

  QApplication::setStyle("fusion");

  app.setOrganizationName("pniekamp");
  app.setOrganizationDomain("au");
  app.setApplicationName("datumdebugviewer");

  try
  {
    DebugViewer viewer;

    viewer.show();

    app.exec();
  }
  catch(exception &e)
  {
    QMessageBox::critical(NULL, "Error", QString("Critical Error: %1").arg(e.what()));
  }
}

#include "debugviewer.moc"
