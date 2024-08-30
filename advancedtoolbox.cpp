#include "advancedtoolbox.h"

#include <QApplication>
#include <QDebug>
#include <QDrag>
#include <QEvent>
#include <QLayoutItem>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QAbstractButton>
#include <QRubberBand>
#include <QStyleOption>
#include <QtMath>
#include <functional>

class ToolBoxPageContainer : public QWidget
{
  public:
    using QWidget::QWidget;
    bool event(QEvent *e)
    {
        if(e->type() == QEvent::LayoutRequest)
            updateGeometry();
        return QWidget::event(e);
    }
};

class ToolBoxTitle : public QAbstractButton
{
    Q_OBJECT
  signals:
    void titleClicked(int index);
    void titleContextMenuRequest(int index, const QPoint &pos);

  public:
    explicit ToolBoxTitle(const QString &label, const QIcon &icon, AdvancedToolBox *parent);
    void setIndex(int index)
    {
        tabIndex = index;
    }
    QSize sizeHint() const;
    QSize minimumSizeHint() const
    {
        return sizeHint();
    }
    void setExpanded(bool expanded)
    {
        if(this->expanded != expanded)
        {
            this->expanded = expanded;
            update();
        }
    }

  protected:
    bool event(QEvent *e);
    void initStyleOption(QStyleOptionToolBox *option) const;
    void paintEvent(QPaintEvent *);
    void changeEvent(QEvent *e)
    {
        _sizeHint = QSize();
        updateGeometry();
        QWidget::changeEvent(e);
    }

  private:
    QPoint pressedPos;
    bool pressed = false;
    bool expanded = true;
    mutable QSize _sizeHint;
    bool hoverBranch = false;
    int tabIndex = -1;
};

class AdvancedToolBoxPrivate : public QObject
{

    Q_DECLARE_PUBLIC(AdvancedToolBox)
  public:
    class ToolBoxItem;

  public:
    AdvancedToolBoxPrivate(AdvancedToolBox *p)
        : QObject()
        , q_ptr(p)
    {
        QStyle *style = q_ptr->style();
        handleWidth = style->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent, nullptr, p);
        indent = style->pixelMetric(QStyle::PM_TreeViewIndentation, nullptr, p);
    }

    ~AdvancedToolBoxPrivate()
    {
        qDeleteAll(items);
    }

    QWidget *takeIndex(int index);
    QWidget *widget(int index);

    void setIndexExpand(int index, bool expand = true);
    void setIndexVisible(int index, bool visible = true);

    void styleChangedEvent();

    void setIndentation(int i);

    void insertWidgetToList(int index, QWidget *widget, const QString &label, const QIcon &icon = QIcon());
    void doLayout();

    void expandStateChanged(int index, bool expand);
    void moveHandle(int index, int distance);
    void updateGeometries(bool animate = false);

    void resetPages();
    ToolBoxSplitterHandle *createHandle();
    ToolBoxTitle *createTitle(const QString &label, const QIcon &icon);
    void showTitleMenu(int index, const QPoint &pos);

    void setDragRubberVisible(bool visible, const QRect &rect = QRect());
    void updateTitleIndent();
    void resetManualSize();
    void widgetDestroyed(QObject *o);
    void resetSizeHint();

  protected:
    int indent = 10;
    int handleWidth = 5;
    QSize minSizeHint;
    QSize sizeHint;
    int boxSpacing = 0;
    QList<ToolBoxItem *> items;

    QRubberBand *dragRubber = nullptr;

    bool isAnimationState = false;
    bool nextIsAnimation = false;
    bool dragSortEnable = true;
    bool animationEnable = true;

    AdvancedToolBox *q_ptr = nullptr;
    friend class AdvancedToolBox;
    friend class ToolBoxSplitterHandle;
};

class AdvancedToolBoxPrivate::ToolBoxItem
{
    QWidget *widget = nullptr;
    ToolBoxSplitterHandle *handle = nullptr; // 移动handle
    ToolBoxTitle *tabTitle = nullptr;        // 标题栏文字、图标等
    QWidget *tabContainer = nullptr;         // 容器，方便做折叠动画

    int layoutHeight = 0;             // 实际布局高度
    QSize sizeHint;                 // 建议高度
    QSize minSize;                  // 最小高度
    QSize maxSize;                  // 最大高度
    int manualHeight = 0;             // 手动高度，用于在调整高度或者展开折叠后做一次缓存，避免resize时抖动

    bool freezeTarget = false;
    bool isExpanded = true;
    inline bool expanded() { return isExpanded; }

    void calItemSize()
    {
        QWidgetItem wi(widget);
        sizeHint = wi.sizeHint();
        minSize = wi.minimumSize();
        maxSize = wi.maximumSize();
        if(sizeHint.height() < 0)
            sizeHint.rheight() = 100;
    }

    int preferHeight()
    {
        int prefer = 0;
        if(manualHeight > 0)
            prefer = manualHeight;
        else if(sizeHint.height() > 0)
            prefer = sizeHint.height();
        return qMin(qMax(minSize.height(), prefer), maxSize.height());
    }

    inline bool canResize() const
    {
        return !widget->isHidden() && isExpanded;
    }

    inline bool isHidden() const
    {
        return widget->isHidden();
    }

    inline QString title() const
    {
        return tabTitle->text();
    }

    friend class AdvancedToolBox;
    friend class AdvancedToolBoxPrivate;
};

using AdToolBoxItem = AdvancedToolBoxPrivate::ToolBoxItem;

AdvancedToolBox::AdvancedToolBox(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new AdvancedToolBoxPrivate(this))
{
    setAcceptDrops(true);
}

AdvancedToolBox::~AdvancedToolBox()
{

}

QSize AdvancedToolBox::sizeHint() const
{
    Q_D(const AdvancedToolBox);
    return d->sizeHint;
}

QSize AdvancedToolBox::minimumSizeHint() const
{
    Q_D(const AdvancedToolBox);
    return d->minSizeHint;
}

void AdvancedToolBox::addWidget(QWidget *widget, const QString &label, const QIcon &icon)
{
    Q_D(AdvancedToolBox);
    Q_ASSERT(widget);
    int n = d->items.count();
    d->insertWidgetToList(n, widget, label, icon);
}

int AdvancedToolBox::indexOf(QWidget *widget)
{
    Q_D(AdvancedToolBox);
    auto &items = d->items;
    for(int i = 0; i < items.size(); i++)
    {
        QWidget *w = items.at(i)->widget;
        if(w == widget)
            return i;
    }
    return -1;
}

QWidget *AdvancedToolBox::takeIndex(int index)
{
    Q_D(AdvancedToolBox);
    return d->takeIndex(index);
}

QWidget *AdvancedToolBox::widget(int index)
{
    Q_D(AdvancedToolBox);
    auto item = d->items.value(index);
    if(item)
        return item->widget;
    return nullptr;
}

void AdvancedToolBox::setItemExpand(int index, bool expand)
{
    Q_D(AdvancedToolBox);
    d->setIndexExpand(index, expand);
}

void AdvancedToolBox::setItemVisible(int index, bool visible)
{
    Q_D(AdvancedToolBox);
    d->setIndexVisible(index, visible);
}

void AdvancedToolBox::setItemText(int index, const QString &text)
{
    Q_D(AdvancedToolBox);
    if(auto item = d->items.value(index))
        item->tabTitle->setText(text);
}

void AdvancedToolBox::setItemIcon(int index, const QIcon &icon)
{
    Q_D(AdvancedToolBox);
    if(auto item = d->items.value(index))
        item->tabTitle->setIcon(icon);
}

QString AdvancedToolBox::itemText(int index)
{
    Q_D(AdvancedToolBox);
    if(auto item = d->items.value(index))
        return item->title();
    return QString();
}

QIcon AdvancedToolBox::itemIcon(int index)
{
    Q_D(AdvancedToolBox);
    if(auto item = d->items.value(index))
        return item->tabTitle->icon();
    return QIcon();
}

int AdvancedToolBox::textIndentation()
{
    Q_D(AdvancedToolBox);
    return d->indent;
}

void AdvancedToolBox::resetTextIndentation(int indent)
{
    Q_D(AdvancedToolBox);
    d->setIndentation(indent);
}

void AdvancedToolBox::setDragSortEnable(bool enable)
{
    Q_D(AdvancedToolBox);
    d->dragSortEnable = enable;
}

void AdvancedToolBox::setAnimationEnable(bool enable)
{
    Q_D(AdvancedToolBox);
    d->animationEnable = enable;
}

bool AdvancedToolBox::event(QEvent *e)
{
    bool ret = QWidget::event(e);
    switch(e->type())
    {
        case QEvent::LayoutRequest:
        {
            Q_D(AdvancedToolBox);
            for(auto item : d->items)
                item->calItemSize();
            d->resetSizeHint();
            d->doLayout();
        }
        break;
        case QEvent::StyleChange:
        {
            Q_D(AdvancedToolBox);
            int old = d->handleWidth;
            d->styleChangedEvent();
            d->updateTitleIndent();
            if(old != d->handleWidth)
            {
                d->resetSizeHint();
                d->doLayout();
            }
        }
        break;
        case QEvent::Resize:
        {
            Q_D(AdvancedToolBox);
            d->doLayout();
        }
        break;
        default:
            break;
    }
    return ret;
}

void AdvancedToolBox::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    Q_D(AdvancedToolBox);
    const int hw = d->handleWidth;
    QStyleOption opt(0);
    opt.state = QStyle::State_None;
    opt.state |= this->isEnabled() ? QStyle::State_Enabled : QStyle::State_None;
    opt.state |= QStyle::State_Horizontal;
    opt.palette = this->palette();

    bool first = true;
    QPainter painter(this);
    for(auto item : d->items)
    {
        if(item->widget->isHidden())
            continue;

        if(!first)
        {
            int top = item->tabTitle->pos().y();
            opt.rect = QRect(0, top - hw, this->width(), hw);
            this->style()->drawPrimitive(QStyle::PE_IndicatorDockWidgetResizeHandle, &opt, &painter, this);
        }
        first = false;
    }
}

void AdvancedToolBox::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *data = event->mimeData();
    if(data->hasFormat("advanced-toolbox-drag-index") && event->source() == this)
    {
        event->acceptProposedAction();
        return;
    }
    return QWidget::dragEnterEvent(event);
}

void AdvancedToolBox::dragMoveEvent(QDragMoveEvent *event)
{
    Q_D(AdvancedToolBox);

    const QMimeData *data = event->mimeData();
    bool ok = false;
    int drag_index = data->data("advanced-toolbox-drag-index").toInt(&ok);
    if(!ok)
    {
        event->ignore();
        return;
    }

    auto &items = d->items;
    const int y = event->pos().y();
    int hover = -1;
    QRect rubber_rect;
    for(int i = 0; i < items.count(); i++)
    {
        auto *item = items.at(i);
        if(item->isHidden())
            continue;
        if(item->expanded())
        {
            QRect cr = item->tabContainer->geometry();
            if(cr.bottom() >= y)
            {
                hover = i;
                QRect tr = item->tabTitle->geometry();
                int mid = (tr.top() + cr.bottom() + 1) / 2;
                int top = mid < y ? mid : tr.top();
                rubber_rect = QRect(tr.x(), top, tr.width(), 0);
                rubber_rect.setBottom(mid < y ? cr.bottom() : mid);
                break;
            }
        }
        else
        {
            QRect r = item->tabTitle->geometry();
            if(r.bottom() >= y)
            {
                hover = i;
                int mid = r.center().y();
                int top = mid < y ? r.bottom() + 1 : r.top() - d->handleWidth;
                rubber_rect = QRect(r.x(), top, r.width(), d->handleWidth);
                if(d->handleWidth <= 1)
                    rubber_rect.adjust(0, -2, 0, 2);
                break;
            }
        }
    }

    if(hover >= 0 && hover != drag_index)
    {
        event->acceptProposedAction();
        d->setDragRubberVisible(true, rubber_rect);
    }
    else
    {
        d->setDragRubberVisible(false);
        event->ignore();
    }
}

void AdvancedToolBox::dropEvent(QDropEvent *event)
{
    Q_D(AdvancedToolBox);
    d->setDragRubberVisible(false);
    const QMimeData *data = event->mimeData();
    bool ok = false;
    int drag_index = data->data("advanced-toolbox-drag-index").toInt(&ok);
    if(!ok)
    {
        event->ignore();
        return;
    }

    auto &items = d->items;
    const int y = event->pos().y();
    int target = -1;
    for(int i = 0; i < items.count(); i++)
    {
        auto *item = items.at(i);
        if(!item->isHidden())
        {
            if(item->expanded())
            {
                QRect cr = item->tabContainer->geometry();
                if(cr.bottom() >= y)
                {
                    QRect tr = item->tabTitle->geometry();
                    int mid = (tr.top() + cr.bottom() + 1) / 2;
                    target = i + (mid < y ? 1 : 0);
                    target -= target > drag_index ? 1 : 0;
                    break;
                }
            }
            else
            {
                QRect r = item->tabTitle->geometry();
                if(r.bottom() >= y)
                {
                    target = i + (r.center().y() < y ? 1 : 0);
                    target -= target > drag_index ? 1 : 0;
                    break;
                }
            }
        }
    }

    if(target >= 0 && target != drag_index)
    {
        event->acceptProposedAction();
        d->items.move(drag_index, target);
        d->resetPages();
        d->updateGeometries();
    }
    else
    {
        event->ignore();
    }
}

void AdvancedToolBox::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_D(AdvancedToolBox);
    d->setDragRubberVisible(false);
    event->ignore();
}

void AdvancedToolBox::startDrag(int index, const QPoint &gpos)
{
    Q_D(AdvancedToolBox);
    if(d->dragSortEnable && index >= 0)
    {
        QDrag drag(this);
        QMimeData *data = new QMimeData();
        data->setData("advanced-toolbox-drag-index", QByteArray::number(index));
        drag.setMimeData(data);

        Q_D(AdvancedToolBox);
        auto item = d->items.at(index);
        QPixmap pix = item->tabTitle->grab();
        QPoint pos = item->tabTitle->mapFromGlobal(gpos);
        drag.setHotSpot(pos);
        drag.setPixmap(pix);

        drag.exec(Qt::MoveAction);
    }
}

QWidget *AdvancedToolBoxPrivate::takeIndex(int index)
{
    Q_Q(AdvancedToolBox);
    ToolBoxItem *item = items.value(index);
    if(item)
    {
        QWidget *ret = item->widget;
        if(ret)
        {
            ret->setVisible(false);
            ret->setParent(q);
        }
        item->tabTitle->deleteLater();
        item->tabContainer->deleteLater();
        item->handle->deleteLater();
        delete item;
        doLayout();
        return ret;
    }
    return nullptr;
}

void AdvancedToolBoxPrivate::setIndexExpand(int index, bool expand)
{
    auto item = items.value(index);
    if(item && item->expanded() != expand)
    {
        item->isExpanded = expand;
        expandStateChanged(index, expand);
    }
}

void AdvancedToolBoxPrivate::setIndexVisible(int index, bool visible)
{
    auto item = items.value(index);
    if(!item)
        return;

    if((item->widget->isHidden() && !visible) || !item->widget->isHidden() && visible)
        return;

    item->widget->setVisible(visible);
    item->tabTitle->setVisible(visible);
    item->tabContainer->setVisible(visible);

    if(!visible && item->isExpanded)
        item->manualHeight = item->layoutHeight;

    resetSizeHint();

    if(visible)
        resetManualSize();

    // 这里选择重新布局，可以考虑尽可能维持该页面后布局，提升体验
    doLayout();
}

void AdvancedToolBoxPrivate::styleChangedEvent()
{
    QStyle *style = q_ptr->style();
    handleWidth = style->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent, nullptr, q_ptr);
    indent = style->pixelMetric(QStyle::PM_TreeViewIndentation, nullptr, q_ptr);
}

void AdvancedToolBoxPrivate::setIndentation(int i)
{
    int old = indent;
    if(i < 0)
        indent = q_ptr->style()->pixelMetric(QStyle::PM_TreeViewIndentation, nullptr, q_ptr);
    else
        indent = i;
    if(old != indent)
    {
        updateTitleIndent();
    }
}

void AdvancedToolBoxPrivate::insertWidgetToList(int index, QWidget *widget, const QString &label, const QIcon &icon)
{
    Q_Q(AdvancedToolBox);
    int count = items.count();
    if(index < 0 || index > count)
        index = count;

    int old_index = q->indexOf(widget);
    if(old_index >= 0) // just move
    {
        if(index != old_index)
        {
            items.move(old_index, index);
            resetPages();
            updateGeometries();
        }
    }
    else
    {
        bool show = !(widget->isHidden() && widget->testAttribute(Qt::WA_WState_ExplicitShowHide));

        ToolBoxItem *item = new ToolBoxItem();
        item->tabContainer = new ToolBoxPageContainer(q);
        widget->setParent(item->tabContainer);
        widget->move(QPoint(0, 0));
        item->widget = widget;
        item->tabTitle = createTitle(label, icon);
        item->handle = createHandle();
        item->isExpanded = true;
        items.insert(index, item);
        if(show)
            widget->show();

        item->calItemSize();
        resetSizeHint();
        connect(widget, &QWidget::destroyed, this, &AdvancedToolBoxPrivate::widgetDestroyed);
        doLayout();
    }
}

void AdvancedToolBoxPrivate::doLayout()
{
    resetPages();
    Q_Q(AdvancedToolBox);

    int space  = 0;
    QVector<ToolBoxItem *> expandedItems;
    {
        int totalSize = 0;
        for(auto item : items)
        {
            if(item->widget->isHidden())
                continue;

            if(item->expanded())
                expandedItems.append(item);

            item->layoutHeight = item->expanded() ? item->preferHeight() : 0;
            totalSize += item->tabTitle->sizeHint().height(); // title height
            totalSize += item->layoutHeight;
            totalSize += handleWidth;
        }
        // handle 要少一个
        totalSize -= (totalSize > 0 ?  handleWidth : 0);
        space = q->rect().height() - totalSize;
    }

    if(space != 0)
    {
        int space2 = space;
        int viewsSize = 0;
        auto it = expandedItems.begin();
        while(it != expandedItems.end())
        {
            ToolBoxItem *item = *it;
            if((space > 0 && item->layoutHeight >= item->maxSize.height()) ||
               (space < 0 && item->layoutHeight <= item->minSize.height()))
            {
                // remove fix size
                it = expandedItems.erase(it);
            }
            else
            {
                viewsSize += item->layoutHeight;
                it++;
            }
        }

        // 空间不足或者有空余时，调整可伸缩的窗口，按照上一次手动调整后的比例，分配或者压缩空间
        // 先尝试按比例分配空间，超过最大、最小的先处理掉。
        bool done = false;
        while(!expandedItems.isEmpty() && !done)
        {
            done = true;
            auto it = expandedItems.begin();
            while(it != expandedItems.end())
            {
                ToolBoxItem *item = *it;
                qreal add = qreal(space2) * item->layoutHeight / viewsSize;
                int prefer = qCeil(item->layoutHeight + add);
                if((space > 0 && prefer > item->maxSize.height()) || (space < 0 && prefer < item->minSize.height()))
                {
                    int threshold = space > 0 ? item->maxSize.height() : item->minSize.height();
                    space2 -= (threshold - item->layoutHeight);
                    viewsSize -= item->layoutHeight;
                    item->layoutHeight = threshold;
                    expandedItems.erase(it);
                    done = false;
                    break;
                }
                it++;
            }
        }
        for(ToolBoxItem *item : expandedItems)
        {
            int add = qCeil(qreal(space2) * item->layoutHeight / viewsSize);
            viewsSize -= item->layoutHeight;
            space2 -= add;
            item->layoutHeight += add;
        }
    }
    updateGeometries();
}

void AdvancedToolBoxPrivate::expandStateChanged(int index, bool expand)
{
    ToolBoxItem *curr = items.value(index);
    if(!curr)
        return;

    curr->tabTitle->setExpanded(expand);
    if(expand)
    {
        int target = curr->preferHeight();
        int space = boxSpacing - target;
        if(space >= 0)
        {
            target += space;
            curr->layoutHeight = qMin(target, curr->maxSize.height());
        }
        else
        {
            space = -space;
            const int count = items.count();
            for(int i = count - 1; i >= 0 && space > 0; i--)
            {
                auto item = items.at(i);
                if(item->canResize() && i != index) // 暂时忽略当前page
                {
                    int diff = qMin(item->layoutHeight - item->minSize.height(), space);
                    item->layoutHeight -= diff;
                    space -= diff;
                }
            }
            curr->layoutHeight = target;
            if(space > 0)
            {
                int diff = qMin(curr->layoutHeight - curr->minSize.height(), space);
                curr->layoutHeight -= diff;
            }
        }
    }
    else
    {
        int space = curr->manualHeight = curr->layoutHeight;
        curr->layoutHeight = 0;
        space += boxSpacing;
        if(space > 0)
        {
            const int count = items.count();
            for(int i = count - 1; i >= 0 && space > 0; i--)
            {
                auto item = items.at(i);
                if(item->canResize())
                {
                    int diff = qMin(item->maxSize.height() - item->layoutHeight, space);
                    item->layoutHeight += diff;
                    space -= diff;
                }
            }
        }
    }

    curr->freezeTarget = true;
    updateGeometries(animationEnable);
    resetManualSize();
}

void AdvancedToolBoxPrivate::moveHandle(int index, int distance)
{
    if(distance == 0)
        return;

    // adjectHandle入口在鼠标移动事件，当鼠标按下时，resetManualSize将当前布局存储
    QVector<ToolBoxItem *> shrink_part, expand_part;
    for(int i = index; i < items.count(); i++)
    {
        auto item = items.at(i);
        if(item->canResize())
            expand_part.append(item);
    }
    for(int i = index - 1; i >= 0; i--)
    {
        auto item = items.at(i);
        if(item->canResize())
            shrink_part.append(item);
    }

    if(distance > 0) // 交换一下
        std::swap(shrink_part, expand_part);

    int expand = 0, shrink = 0;
    for(ToolBoxItem *item : shrink_part)
        shrink += item->manualHeight - item->minSize.height();

    for(ToolBoxItem *item : expand_part)
        expand += item->maxSize.height() - item->manualHeight;

    int min_dis = qMin(qMin(shrink, expand), abs(distance));
    if(min_dis == 0)
        return;

    //调整
    int space = min_dis;
    for(int i = 0; i < shrink_part.count(); i++)
    {
        ToolBoxItem *item = shrink_part.at(i);
        int diff = qMin(item->manualHeight - item->minSize.height(), space);
        item->layoutHeight = item->manualHeight - diff;
        space -= diff;
    }

    space = min_dis;
    for(int i = 0; i < expand_part.count(); i++)
    {
        ToolBoxItem *item = expand_part.at(i);
        int diff = qMin(item->maxSize.height() - item->manualHeight, space);
        item->layoutHeight = item->manualHeight + diff;
        space -= diff;
    }
    updateGeometries();
}

// 根据计算好的布局，设置窗口位置等
void AdvancedToolBoxPrivate::updateGeometries(bool animate)
{
    Q_Q(AdvancedToolBox);
    animate = animate && q->isVisible();
    QParallelAnimationGroup *group = nullptr;
    if(isAnimationState)
    {
        nextIsAnimation = animate;
        return;
    }
    if(animate)
    {
        isAnimationState = true;
        group = new QParallelAnimationGroup();
        QObject::connect(group, &QParallelAnimationGroup::finished, this,
                         [this](){
                             isAnimationState = false;
                             updateGeometries(nextIsAnimation);
                         });
    }

    const int hw = handleWidth;
    QRect cr = q->rect();
    int x = cr.left(), offset = cr.top(), width = cr.width();
    bool first = true;
    for(auto item : items)
    {
        if(item->isHidden())
            continue;

        int th = item->tabTitle->sizeHint().height();
        offset += (first ? 0 : hw);
        offset += th;

        int h = item->layoutHeight;
        QRect start = item->tabContainer->geometry();
        QRect end(x, offset, width, h);

        bool freezeSize = item->freezeTarget;
        auto resizeTo = [item, th, hw, freezeSize](const QVariant &val)
        {
            QRect rect = val.toRect();
            item->tabContainer->setGeometry(rect);
            if(!freezeSize)
            {
                item->widget->setGeometry(QRect(QPoint(0, 0), rect.size()));
            }
            rect = QRect(rect.left(), rect.top() - th, rect.width(), th);
            item->tabTitle->setGeometry(rect);

            if(item->handle->isVisible())
            {
                rect = QRect(rect.left(), rect.top() - hw, rect.width(), hw);
                if(hw <= 1)
                    rect.adjust(0, -2, 0, 2);
                item->handle->setGeometry(rect);
            }
        };
        if(freezeSize && item->expanded())
        {
            item->widget->resize(end.size());
        }

        if(animate && start != end)
        {
            QVariantAnimation *animation = new QVariantAnimation(group);
            animation->setDuration(100);
            animation->setStartValue(start);
            animation->setEndValue(end);
            connect(animation, &QVariantAnimation::valueChanged, this, resizeTo);
            group->addAnimation(animation);
        }
        else
        {
            resizeTo(end);
        }
        item->freezeTarget = false;
        offset += h;
        first = false;
    }
    boxSpacing = cr.bottom() - (offset - 1);
    if(group)
    {
        connect(group, &QVariantAnimation::finished, this, &AdvancedToolBoxPrivate::resetSizeHint);
        group->start(QAbstractAnimation::DeleteWhenStopped);
    }
    else
    {
        resetSizeHint();
    }
    nextIsAnimation = false;
}

// 更新handle顺序以及重新设置隐藏和显示
void AdvancedToolBoxPrivate::resetPages()
{
    int count = items.count();
    bool visible = false;
    for(int i = 0; i < count; i++)
    {
        auto item = items.at(i);
        item->tabTitle->setIndex(i);
        item->handle->setIndex(i);
        if(item->isHidden())
        {
            item->tabTitle->hide();
            item->tabContainer->hide();
            item->handle->setVisible(false);
        }
        else
        {
            item->tabTitle->show();
            item->tabContainer->show();
            item->handle->setVisible(visible); // 将第一个handle隐藏
            visible = true;
        }
    }
}

ToolBoxSplitterHandle *AdvancedToolBoxPrivate::createHandle()
{
    Q_Q(AdvancedToolBox);
    ToolBoxSplitterHandle *handle = new ToolBoxSplitterHandle(q);
    handle->setAttribute(Qt::WA_MouseNoMask, true);
    handle->setAutoFillBackground(false);
    handle->setVisible(false);
    handle->setCursor(Qt::SizeVerCursor);
    // handle->installEventFilter(this);
    return handle;
}

ToolBoxTitle *AdvancedToolBoxPrivate::createTitle(const QString &label, const QIcon &icon)
{
    Q_Q(AdvancedToolBox);
    ToolBoxTitle *title = new ToolBoxTitle(label, icon, q);

    QObject::connect(title, &ToolBoxTitle::titleClicked, this, [this](int index)
                     {
                         auto item = items.value(index);
                         if(item)
                             setIndexExpand(index, !item->isExpanded); });

    QObject::connect(title, &ToolBoxTitle::titleContextMenuRequest, this, &AdvancedToolBoxPrivate::showTitleMenu);
    return title;
}

void AdvancedToolBoxPrivate::showTitleMenu(int index, const QPoint &gpos)
{
    Q_Q(AdvancedToolBox);
    QMenu *menu = new QMenu(q);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *hide_action = menu->addAction(tr("Hide"));
    menu->addSeparator();
    int visible_count = 0;
    QAction *first_checked = nullptr;
    for(int i = 0; i < items.count(); i++)
    {
        auto item = items.at(i);
        const bool hidden = item->isHidden();
        QAction *action = menu->addAction(item->title());
        action->setCheckable(true);
        action->setChecked(!hidden);
        if(!hidden)
        {
            visible_count++;
            if(!first_checked)
                first_checked = action;
            if(index == i)
                QObject::connect(hide_action, SIGNAL(triggered(bool)), action, SLOT(trigger()));
        }
        auto bind_slot = std::bind(&AdvancedToolBoxPrivate::setIndexVisible, this, i, std::placeholders::_1);
        QObject::connect(action, &QAction::triggered, this, bind_slot);
    }

    hide_action->setVisible(index >= 0);
    hide_action->setEnabled(visible_count > 1);
    if(visible_count == 1 && first_checked)
    {
        first_checked->setEnabled(false);
    }
    menu->exec(gpos);
}

void AdvancedToolBoxPrivate::setDragRubberVisible(bool visible, const QRect &rect)
{
    if(!dragRubber && visible)
    {
        Q_Q(AdvancedToolBox);
        dragRubber = new QRubberBand(QRubberBand::Rectangle, q);
    }
    if(visible)
    {
        dragRubber->setGeometry(rect);
        dragRubber->show();
    }
    else if(dragRubber)
    {
        dragRubber->hide();
    }
}

void AdvancedToolBoxPrivate::updateTitleIndent()
{
    for(auto item : items)
    {
        item->tabTitle->update();
    }
}

// 将当前当前布局尺寸保存
// 手动调整分割线位置、折叠展开，都会触发该逻辑
void AdvancedToolBoxPrivate::resetManualSize()
{
    for(auto item : items)
    {
        if(item->canResize())
        {
            item->manualHeight = item->layoutHeight;
        }
    }
}

void AdvancedToolBoxPrivate::widgetDestroyed(QObject *o)
{
    for(int i = 0; i < items.size(); i++)
    {
        if(items.at(i)->widget == o)
        {
            takeIndex(i);
            break;
        }
    }
}

void AdvancedToolBoxPrivate::resetSizeHint()
{
    int handle_h = 0;
    QSize min_size  = QSize(0, 0);
    QSize size = QSize(0, 0);
    for(auto item : items)
    {
        if(item->widget->isHidden())
            continue;
        if(item->expanded())
        {
            min_size.rheight() += item->minSize.height();
            min_size.rwidth() =  std::max(item->minSize.width(), min_size.width());

            size.rheight() += item->sizeHint.height();
            size.rwidth() =  std::max(item->sizeHint.width(), size.width());
        }
        {
            QSize title = item->tabTitle->sizeHint();
            min_size.rheight() += title.height();
            size.rheight() += title.height();
        }
        handle_h += handleWidth;
    }
    min_size.rheight() += std::max(handle_h - handleWidth, 0);
    size.rheight() += std::max(handle_h - handleWidth, 0);
    if(minSizeHint != min_size || sizeHint != size)
    {
        minSizeHint = min_size;
        sizeHint = size;
        Q_Q(AdvancedToolBox);
        q->updateGeometry();
    }
}

ToolBoxSplitterHandle::ToolBoxSplitterHandle(AdvancedToolBox *parent)
    : QWidget(parent)
{
}

void ToolBoxSplitterHandle::setIndex(int index)
{
    _index = index;
}

int ToolBoxSplitterHandle::index()
{
    return _index;
}

void ToolBoxSplitterHandle::mouseMoveEvent(QMouseEvent *event)
{
    if(pressed)
    {
        QPoint pos = event->globalPos();
        AdvancedToolBox *box = static_cast<AdvancedToolBox *>(parentWidget());
        box->d_ptr->moveHandle(_index, pos.y() - moveStart.y());
    }
}

void ToolBoxSplitterHandle::mousePressEvent(QMouseEvent *event)
{
    if(event->buttons() == Qt::LeftButton)
    {
        pressed = true;
        moveStart = event->globalPos();
        AdvancedToolBox *box = static_cast<AdvancedToolBox *>(parentWidget());
        box->d_ptr->resetManualSize();
    }
}

void ToolBoxSplitterHandle::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        pressed = false;
        AdvancedToolBox *box = static_cast<AdvancedToolBox *>(parentWidget());
        box->d_ptr->resetManualSize();
    }
}



ToolBoxTitle::ToolBoxTitle(const QString &label, const QIcon &icon, AdvancedToolBox *parent)
    : QAbstractButton(parent)
{
    setText(label);
    setIcon(icon);
    setIconSize(QSize(16, 16));
    connect(this, &ToolBoxTitle::clicked, this, [this]()
            { emit titleClicked(tabIndex); });
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &ToolBoxTitle::customContextMenuRequested, this, [this](const QPoint &pos)
            { emit titleContextMenuRequest(tabIndex, this->mapToGlobal(pos)); });

    setAttribute(Qt::WA_Hover);
}

QSize ToolBoxTitle::sizeHint() const
{
    if(_sizeHint.isValid())
        return _sizeHint;

    ensurePolished();
    QStyleOptionTab opt;
    opt.initFrom(this);
    if(this->expanded)
        opt.state |= QStyle::State_On;
    if(this->underMouse())
        opt.state |= QStyle::State_MouseOver;

    QWidget *parent = parentWidget();
    int w = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, parent);
    int h = style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, parent);

    bool nullicon = this->icon().isNull();
    QSize icon_size = nullicon ? QSize(0, 0) : this->iconSize();
    w += icon_size.width();
    w += nullicon ? 0 : 4;

    const QFontMetrics fm = fontMetrics();
    w += fm.size(0, this->text()).width();
    h += qMax(fm.height(), icon_size.height());
    _sizeHint = style()->sizeFromContents(QStyle::CT_TabBarTab, &opt, QSize(w, h), parent);
    return _sizeHint;
}

bool ToolBoxTitle::event(QEvent *e)
{
    switch(e->type())
    {
    case QEvent::HoverMove:
    case QEvent::HoverEnter:
    {
        int indent = static_cast<AdvancedToolBox *>(parentWidget())->textIndentation();
        QHoverEvent *he = static_cast<QHoverEvent *>(e);
        QRect rect = this->rect();
        rect.setRight(rect.left() + indent);
        bool test = rect.contains(he->pos());
        if(hoverBranch != test)
        {
            hoverBranch = test;
            update();
        }
    }
    break;
    case QEvent::HoverLeave:
        hoverBranch = false;
        break;
    case QEvent::MouseButtonPress:
    {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        if(me->buttons() == Qt::LeftButton)
        {
            pressed = true;
            pressedPos = me->globalPos();
        }
    }
    break;
    case QEvent::MouseMove:
        if(pressed)
        {
            QMouseEvent *me = static_cast<QMouseEvent *>(e);
            if((me->globalPos() - pressedPos).manhattanLength() > QApplication::startDragDistance())
            {
                setDown(false);
                qobject_cast<AdvancedToolBox *>(parentWidget())->startDrag(tabIndex, pressedPos);
                return true;
            }
        }
        break;
    case QEvent::MouseButtonRelease:
        if(static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)
            pressed = false;
        break;
    default:
        break;
    }
    return QAbstractButton::event(e);
}

void ToolBoxTitle::initStyleOption(QStyleOptionToolBox *option) const
{
    if(!option)
        return;
    option->initFrom(this);

    option->text = text();
    option->icon = icon();
    if(isDown())
        option->state |= QStyle::State_Sunken;
    if(this->expanded)
        option->state |= QStyle::State_Open;
}

void ToolBoxTitle::paintEvent(QPaintEvent *)
{
    QWidget *parent = parentWidget();

    QStyleOptionToolBox tabopt;
    initStyleOption(&tabopt);

    QPainter painter(this);
    style()->drawControl(QStyle::CE_ToolBoxTabShape, &tabopt, &painter, parent);

    int indent = static_cast<AdvancedToolBox *>(parentWidget())->textIndentation();
    // draw branch
    if(indent > 0)
    {
        QStyleOptionViewItem branchopt;
        branchopt.rect = tabopt.rect;
        branchopt.state = tabopt.state;
        branchopt.state &= ~QStyle::State_MouseOver;
        branchopt.state |= hoverBranch ? QStyle::State_MouseOver : QStyle::State_None;
        branchopt.state |= QStyle::State_Children;
        branchopt.rect.setRight(tabopt.rect.left() + indent);
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &branchopt, &painter, parent);
    }
    tabopt.rect.setLeft(tabopt.rect.left() + indent);

    // draw icon
    bool null_icon = this->icon().isNull();
    int icon_width = this->iconSize().width();
    if(!null_icon)
    {
        QRect cr = style()->subElementRect(QStyle::SE_ToolBoxTabContents, &tabopt, parent);
        cr.setWidth(icon_width + 2);
        cr.moveLeft(cr.left() + 2);
        QIcon::Mode mode = tabopt.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled;
        if(mode == QIcon::Normal && tabopt.state & QStyle::State_HasFocus)
            mode = QIcon::Active;
        QIcon::State state = tabopt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;
        this->icon().paint(&painter, cr, Qt::AlignCenter, mode, state);
        tabopt.rect.adjust(icon_width + 4, 0, 0, 0);
    }

    // draw text
    if(!tabopt.text.isEmpty())
    {
        tabopt.icon = QIcon();
        // QStyleOptionToolBox固定左对齐。可以考虑使用 QStyleOptionTab 绘制文本，以支持样式对齐，不过默认是居中样式。
        style()->drawControl(QStyle::CE_ToolBoxTabLabel, &tabopt, &painter, parent);
    }
}

#include "advancedtoolbox.moc"
