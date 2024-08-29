#include "widget.h"
#include "ui_widget.h"
#include <QAction>
#include <QDebug>
#include <QScrollArea>
#include <QTimer>
#include "advancedtoolbox.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    QScrollArea * scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    toolBox = new AdvancedToolBox(this);
    QFrame * frame = new QFrame(toolBox);
    frame->setStyleSheet("QFrame{background:#E8BD92;}");
    frame->setMaximumHeight(300);

    QIcon iconfolder(":/images/folder.svg");
    iconfolder.addFile(":/images/reopen-folder.svg", QSize(), QIcon::Normal, QIcon::On);
    toolBox->addWidget(frame, "AAA",iconfolder);

    frame = new QFrame(toolBox);
    frame->setStyleSheet("QFrame{background:#BDE8A7;}");
    frame->setMinimumHeight(200);
    frame->setMinimumWidth(500);
    toolBox->addWidget(frame, "BBB", QIcon(":/images/smile.png"));

    QWidget * btn = new QPushButton("abc", toolBox);
    btn->setStyleSheet("QFrame{background:#78B294;}");
    toolBox->addWidget(btn, "CCC", QIcon(":/images/user.png"));

    frame = new QFrame(toolBox);
    frame->setMinimumHeight(200);
    frame->setStyleSheet("QFrame{background:#82C7D9;}");
    toolBox->addWidget(frame, "DDD");

    scroll->setWidget(toolBox);
    ui->verticalLayout->insertWidget(1, scroll, 1);
}

Widget::~Widget()
{
    delete ui;
}
