#ifndef ADVANCEDTOOLBOX_H
#define ADVANCEDTOOLBOX_H

#include <QFrame>
#include <QWidget>
#include <QIcon>

class AdvancedToolBoxPrivate;
class ToolBoxTitle;
class ToolBoxSplitterHandle;

class AdvancedToolBox : public QWidget
{
    Q_OBJECT
public:
    explicit AdvancedToolBox(QWidget *parent = nullptr);
    void addWidget(QWidget * widget, const QString & label, const QIcon & icon = QIcon());
    int indexOf(QWidget * widget);
    QWidget * takeIndex(int index);
    QWidget * widget(int index);

    void setItemExpand(int index, bool expand = true);
    void setItemVisible(int index, bool visible = true);

    void setItemText(int index, const QString & text);
    void setItemIcon(int index, const QIcon & icon);
    QString itemText(int index);
    QIcon itemIcon(int index);

    int textIndentation();
    void resetTextIndentation(int indent = -1);

protected:
    bool event(QEvent *e);
    void paintEvent(QPaintEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void startDrag(int index, const QPoint & gpos);

private:
    Q_DECLARE_PRIVATE(AdvancedToolBox)
    Q_DISABLE_COPY(AdvancedToolBox)
    QScopedPointer<AdvancedToolBoxPrivate> d_ptr;

private:
    friend class ToolBoxTitle;
    friend class ToolBoxSplitterHandle;
};

class ToolBoxSplitterHandle : public QWidget
{
public:
    ToolBoxSplitterHandle(AdvancedToolBox * parent);
    void setIndex(int index);
    int index();
protected:
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    int _index = -1;
    bool pressed = false;
    QPoint moveStart;
};

#endif // ADVANCEDTOOLBOX_H
