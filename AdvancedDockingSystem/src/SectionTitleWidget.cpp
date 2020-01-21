#include "ads/SectionTitleWidget.h"

#include <QString>
#include <QApplication>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QMimeData>
#include <QDrag>
#include <QCursor>
#include <QStyle>
#include <QSplitter>
#include <QLabel>
#include <QPainter>

#ifdef ADS_ANIMATIONS_ENABLED
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#endif

#include "ads/Internal.h"
#include "ads/DropOverlay.h"
#include "ads/SectionContent.h"
#include "ads/SectionWidget.h"
#include "ads/FloatingWidget.h"
#include "ads/ContainerWidget.h"

ADS_NAMESPACE_BEGIN

SectionTitleWidget::SectionTitleWidget(SectionContent::RefPtr content, QWidget* parent) :
	QFrame(parent),
	_content(content),
	_tabMoving(false),
	_activeTab(false)
{
	QBoxLayout* l = new QBoxLayout(QBoxLayout::LeftToRight);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	l->addWidget(content->titleWidget());
	setLayout(l);
}

SectionTitleWidget::~SectionTitleWidget()
{
	layout()->removeWidget(_content->titleWidget());
}

bool SectionTitleWidget::isActiveTab() const
{
	return _activeTab;
}

void SectionTitleWidget::setActiveTab(bool active)
{
	if (active != _activeTab)
	{
		_activeTab = active;

		style()->unpolish(this);
		style()->polish(this);
		update();

		emit activeTabChanged();
	}
}

void SectionTitleWidget::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		ev->accept();
		_dragStartPos = ev->pos();
		return;
	}
	QFrame::mousePressEvent(ev);
}

void SectionTitleWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    SectionWidget* section = findParentSectionWidget(this);
	ContainerWidget* cw = findParentContainerWidget(this);

	// Drop contents of FloatingWidget into SectionWidget.
    if (_dw)
	{
        _dw->deleteLater();
        _dw = nullptr;

		SectionWidget* sw = cw->sectionAt(cw->mapFromGlobal(ev->globalPos()));
		if (sw)
		{
			cw->_dropOverlay->setAllowedAreas(ADS_NS::AllAreas);
			DropArea loc = cw->_dropOverlay->showDropOverlay(sw);
			if (loc != InvalidDropArea)
			{
				InternalContentData data;
                section->takeContent(_content->uid(), data);
                if(section->contents().isEmpty()){
                    delete section;
                    section = nullptr;
                }

                cw->dropContent(data, sw, loc, true);
                deleteEmptySplitter(cw);
            }
        }
		// Mouse is over a outer-edge drop area
		else
		{
			DropArea dropArea = ADS_NS::InvalidDropArea;
			if (cw->outerTopDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
				dropArea = ADS_NS::TopDropArea;
			if (cw->outerRightDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
				dropArea = ADS_NS::RightDropArea;
			if (cw->outerBottomDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
				dropArea = ADS_NS::BottomDropArea;
			if (cw->outerLeftDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
				dropArea = ADS_NS::LeftDropArea;

			if (dropArea != ADS_NS::InvalidDropArea)
			{
                InternalContentData data;
                section->takeContent(_content->uid(), data);
                if(section->contents().isEmpty()){
                    delete section;
                    section = nullptr;
                }

                cw->dropContent(data, NULL, dropArea, true);
                deleteEmptySplitter(cw);
            }
		}
	}
    else if(_fw)
    {
        SectionWidget* sw = cw->sectionAt(cw->mapFromGlobal(ev->globalPos()));
        if (sw)
        {
            cw->_dropOverlay->setAllowedAreas(ADS_NS::AllAreas);
            DropArea loc = cw->_dropOverlay->showDropOverlay(sw);
            if (loc != InvalidDropArea)
            {
                InternalContentData data;
                _fw->takeContent(data);
                _fw->deleteLater();
                _fw = nullptr;
                cw->dropContent(data, sw, loc, true);
            }
        }
        // Mouse is over a outer-edge drop area
        else
        {
            DropArea dropArea = ADS_NS::InvalidDropArea;
            if (cw->outerTopDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
                dropArea = ADS_NS::TopDropArea;
            if (cw->outerRightDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
                dropArea = ADS_NS::RightDropArea;
            if (cw->outerBottomDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
                dropArea = ADS_NS::BottomDropArea;
            if (cw->outerLeftDropRect().contains(cw->mapFromGlobal(ev->globalPos())))
                dropArea = ADS_NS::LeftDropArea;

            if (dropArea != ADS_NS::InvalidDropArea)
            {
                InternalContentData data;
                _fw->takeContent(data);
                _fw->deleteLater();
                _fw = nullptr;
                cw->dropContent(data, NULL, dropArea, true);
            }
        }
    }
	// End of tab moving, change order now
    else if (_tabMoving && section != NULL)
	{
		// Find tab under mouse
		QPoint pos = ev->globalPos();
		pos = section->mapFromGlobal(pos);
		const int fromIndex = section->indexOfContent(_content);
		const int toIndex = section->indexOfContentByTitlePos(pos, this);
		section->moveContent(fromIndex, toIndex);
	}

	if (!_dragStartPos.isNull())
		emit clicked();

	// Reset
	_dragStartPos = QPoint();
	_tabMoving = false;
	cw->_dropOverlay->hideDropOverlay();
	QFrame::mouseReleaseEvent(ev);
}

void SectionTitleWidget::mouseMoveEvent(QMouseEvent* ev)
{
	ContainerWidget* cw = findParentContainerWidget(this);
	SectionWidget* section = NULL;

	// Move already existing FloatingWidget
    if (ev->buttons() & Qt::LeftButton)
	{
        if(_dw)
        {
            ev->accept();

            const QPoint moveToPos = ev->globalPos() - _dragStartPos;
            _dw->move(moveToPos);

            // Mouse is over a SectionWidget
            section = cw->sectionAt(cw->mapFromGlobal(QCursor::pos()));
            // Show drop indicator
            if (section)
            {
                if(section->contents().size() == 1 && section->contents().first() == _content){
                    return;
                }
                cw->_dropOverlay->setAllowedAreas(ADS_NS::AllAreas);
                cw->_dropOverlay->showDropOverlay(section);
            }
            // Mouse is at the edge of the ContainerWidget
            // Top, Right, Bottom, Left
            else if (cw->outerTopDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
            {
                cw->_dropOverlay->setAllowedAreas(ADS_NS::TopDropArea);
                cw->_dropOverlay->showDropOverlay(cw, cw->outerTopDropRect());
            }
            else if (cw->outerRightDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
            {
                cw->_dropOverlay->setAllowedAreas(ADS_NS::RightDropArea);
                cw->_dropOverlay->showDropOverlay(cw, cw->outerRightDropRect());
            }
            else if (cw->outerBottomDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
            {
                cw->_dropOverlay->setAllowedAreas(ADS_NS::BottomDropArea);
                cw->_dropOverlay->showDropOverlay(cw, cw->outerBottomDropRect());
            }
            else if (cw->outerLeftDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
            {
                cw->_dropOverlay->setAllowedAreas(ADS_NS::LeftDropArea);
                cw->_dropOverlay->showDropOverlay(cw, cw->outerLeftDropRect());
            }
            else
            {
                cw->_dropOverlay->hideDropOverlay();
            }
        }
        else if(_fw)
        {
            ev->accept();

            const QPoint moveToPos = ev->globalPos() - (_dragStartPos + QPoint(ADS_WINDOW_FRAME_BORDER_WIDTH, ADS_WINDOW_FRAME_BORDER_WIDTH));
            _fw->move(moveToPos);

            // Show drop indicator
            if (true)
            {
                // Mouse is over a SectionWidget
                section = cw->sectionAt(cw->mapFromGlobal(QCursor::pos()));
                if (section)
                {
                    cw->_dropOverlay->setAllowedAreas(ADS_NS::AllAreas);
                    cw->_dropOverlay->showDropOverlay(section);
                }
                // Mouse is at the edge of the ContainerWidget
                // Top, Right, Bottom, Left
                else if (cw->outerTopDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
                {
                    cw->_dropOverlay->setAllowedAreas(ADS_NS::TopDropArea);
                    cw->_dropOverlay->showDropOverlay(cw, cw->outerTopDropRect());
                }
                else if (cw->outerRightDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
                {
                    cw->_dropOverlay->setAllowedAreas(ADS_NS::RightDropArea);
                    cw->_dropOverlay->showDropOverlay(cw, cw->outerRightDropRect());
                }
                else if (cw->outerBottomDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
                {
                    cw->_dropOverlay->setAllowedAreas(ADS_NS::BottomDropArea);
                    cw->_dropOverlay->showDropOverlay(cw, cw->outerBottomDropRect());
                }
                else if (cw->outerLeftDropRect().contains(cw->mapFromGlobal(QCursor::pos())))
                {
                    cw->_dropOverlay->setAllowedAreas(ADS_NS::LeftDropArea);
                    cw->_dropOverlay->showDropOverlay(cw, cw->outerLeftDropRect());
                }
                else
                {
                    cw->_dropOverlay->hideDropOverlay();
                }
            }
        }
        // Begin to drag/float the SectionContent.
        else if (!_dragStartPos.isNull()
                 && (section = findParentSectionWidget(this)) != NULL
                 && !section->titleAreaGeometry().contains(section->mapFromGlobal(ev->globalPos())))
        {
            ev->accept();
            if(_content->flags().testFlag(SectionContent::Floatable)){
                // Create floating widget.
                InternalContentData data;
                if (!section->takeContent(_content->uid(), data))
                {
                    qWarning() << "THIS SHOULD NOT HAPPEN!!" << _content->uid() << _content->uniqueName();
                    return;
                }

                _fw = new FloatingWidget(cw, data.content, data.titleWidget, data.contentWidget, cw);
                _fw->resize(section->size());
                cw->_floatings.append(_fw); // Note: I don't like this...

                const QPoint moveToPos = ev->globalPos() - (_dragStartPos + QPoint(ADS_WINDOW_FRAME_BORDER_WIDTH, ADS_WINDOW_FRAME_BORDER_WIDTH));
                _fw->move(moveToPos);
                _fw->show();

                // Delete old section, if it is empty now.
                if (section->contents().isEmpty())
                {
                    delete section;
                    section = NULL;
                }
                deleteEmptySplitter(cw);
            }
            else{
                _dw = new QLabel(cw, Qt::FramelessWindowHint | Qt::Tool);
                auto pixmap = grab(rect());
                _dw->resize(pixmap.size());
                static_cast<QLabel *>(_dw.data())->setPixmap(pixmap);
                _dw->setWindowOpacity(0.8);

                const QPoint moveToPos = ev->globalPos() - _dragStartPos;
                _dw->move(moveToPos);
                _dw->show();

                if(_tabMoving){
                    // Find tab under mouse
                    QPoint pos = ev->globalPos();
                    pos = section->mapFromGlobal(pos);
                    const int fromIndex = section->indexOfContent(_content);
                    const int toIndex = section->indexOfContentByTitlePos(pos, this);
                    section->moveContent(fromIndex, toIndex);
                }
            }
        }
        // Begin to drag title inside the title area to switch its position inside the SectionWidget.
        else if (!_dragStartPos.isNull()
                 && (ev->pos() - _dragStartPos).manhattanLength() >= QApplication::startDragDistance() // Wait a few pixels before start moving
                 && (section = findParentSectionWidget(this)) != NULL
                 && section->titleAreaGeometry().contains(section->mapFromGlobal(ev->globalPos())))
        {
            ev->accept();

            _tabMoving = true;
            raise(); // Raise current title-widget above other tabs
        }
        return;
	}
	// Handle movement of this tab
	else if (_tabMoving
			&& (section = findParentSectionWidget(this)) != NULL)
	{
		ev->accept();

		int left, top, right, bottom;
		getContentsMargins(&left, &top, &right, &bottom);
		QPoint moveToPos = mapToParent(ev->pos()) - _dragStartPos;
		moveToPos.setY(0/* + top*/);
		move(moveToPos);

		return;
	}
    QFrame::mouseMoveEvent(ev);
}

ADS_NAMESPACE_END
