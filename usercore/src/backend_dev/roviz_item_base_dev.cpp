
#include "backend_dev/roviz_item_base_dev.h"
#include "backend_dev/roviz_item_base_dev_p.h"

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "helper/settings_scope.h"
#include "item/item_input.h"
#include "item/item_output.h"
#include "core/template_decl.h"
#include "backend_dev/trim_impl_dev.h"
#include "gui/shared_window.h"
#include "gui/stream_widget.h"
#include "backend_dev/config_impl_dev.h"
#include "opencv/cv.h"
#include "streams/all_streams.h"

RovizItemBaseDev::RovizItemBaseDev(std::string type_name)
    : AbstractItem(QString::fromStdString(type_name)),
      _this(new RovizItemBaseDevPrivate(this))
{
    // Each item can display a widget in the shared window.
    // That's prepared here.
    _this->main_widget = new QWidget();
    _this->main_layout = new QHBoxLayout();
    _this->main_control_layout = new QHBoxLayout();
    _this->main_image_layout = new QHBoxLayout();
    _this->control_base = new QWidget();
    _this->collapse_btn = new QPushButton();
    QHBoxLayout *hlayout = new QHBoxLayout();

    _this->control_base->setLayout(_this->main_control_layout);
    _this->collapse_btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    _this->collapse_btn->setFixedWidth(20);
    _this->collapse_btn->setFixedHeight(20);
    _this->collapse_btn->setText("-");
    hlayout->addWidget(_this->collapse_btn);
    hlayout->addWidget(_this->control_base);
    _this->main_layout->addLayout(hlayout);
    connect(_this->collapse_btn, &QPushButton::clicked,
            _this.data(), &RovizItemBaseDevPrivate::collapseBtnClicked);

    _this->main_layout->addLayout(_this->main_image_layout);
    _this->main_widget->setLayout(_this->main_layout);

    hlayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _this->main_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _this->main_control_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _this->main_image_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    hlayout->setAlignment(_this->collapse_btn, Qt::AlignTop | Qt::AlignLeft);

    // Hide the collapse button as long as there is nothing to collapse
    _this->collapse_btn->hide();
    connect(this->settingsScope(), &SettingsScope::parentScopeChanged,
            _this.data(), &RovizItemBaseDevPrivate::parentScopeChanged);
    _this->main_widget->hide();
    _this->config_present = false;
    cv::setBreakOnError(true);
}

RovizItemBaseDev::~RovizItemBaseDev()
{
    if(this->settingsScope()->parentScope() != nullptr)
        SharedWindow::instance(this->settingsScope()->parentScope())->removeItem(this);

    // Just to be sure
    for(auto &widget : _this->output_widgets)
        widget->qwidget()->deleteLater();
}

QWidget *RovizItemBaseDev::widget()
{
    // The main widget contains the trims as well as all output stream widgets
    return _this->main_widget;
}

void RovizItemBaseDev::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // In case AbstractItem also implements this
    AbstractItem::mouseDoubleClickEvent(event);
    auto x = this->settingsScope()->parentScope()->allSettings();
    SharedWindow::instance(this->settingsScope()->parentScope())->load();
    _this->main_widget->parentWidget()->show();
    _this->main_widget->show();
}

void RovizItemBaseDev::contextMenuPrepare(QMenu &menu) const
{
    if(_this->config_present)
        menu.addAction("Configure", [this]{_this->showConfigWindow();});
}

void RovizItemBaseDev::stop()
{
    for(StreamWidget* widget : _this->output_widgets)
        widget->reset();
}

void RovizItemBaseDev::restart()
{
    // Restarting doesn't mean starting it when it was stopped before
    if(this->running())
    {
        this->stop();
        this->start();
    }
}

template<class T>
Input<T> RovizItemBaseDev::addInputBase(std::string name, RovizItem *item)
{
    ItemInput *in;
    Input<T> input(item);
    InputPrivate *in_p = input._this.get();

    in = this->AbstractItem::addInput(qMetaTypeId<T>(), QString::fromStdString(name));

    connect(in, &ItemInput::dataChanged,
            this, [in, in_p]()
    {
        if(in->data() == nullptr)
            in_p->disconnect();
        else
            in_p->connect(dynamic_cast<StreamWidget*>(in->data())->output());
    });

    return input;
}

template<class T>
Output<T> RovizItemBaseDev::addOutputBase(std::string name)
{
    ItemOutput *item_output;
    StreamWidget *widget;
    Output<T> output;
    OutputPrivate *output_p = output._this.get();

    widget = T::initWidget(output_p);
    // TODO Temporary hack
    if(widget == nullptr)
        return output;

    _this->main_image_layout->addWidget(widget->qwidget());
    _this->output_widgets.append(widget);

    // We set the widget as the output data because we need a QObject for that and that's
    // way easier than making OutputPrivate a QObject
    item_output = this->AbstractItem::addOutput(qMetaTypeId<T>(), QString::fromStdString(name));
    this->setOutputData(item_output, widget->qwidget());

    output_p->connect(nullptr, [widget](StreamObject obj){widget->newObject(obj);});

    return output;
}

TrimImpl *RovizItemBaseDev::getTrimImpl(std::string name, double default_value, double min, double max, int steps, bool logarithmic, std::function<void (double)> notifier_func)
{
    TrimImplDev *impl = new TrimImplDev(this, name, default_value, min, max, steps, logarithmic, notifier_func);

    _this->main_control_layout->addLayout(impl->layout());
    _this->collapse_btn->show();

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<int>::type &default_value, int min, int max, bool restart_when_changed)
{
    ConfigImplDev<int> *impl = new ConfigImplDev<int>(this, name, default_value, min, max, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<double>::type &default_value, double min, double max, bool restart_when_changed)
{
    ConfigImplDev<double> *impl = new ConfigImplDev<double>(this, name, default_value, min, max, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<std::string>::type &default_value, std::function<bool (std::string &)> checker, bool restart_when_changed)
{
    ConfigImplDev<std::string> *impl = new ConfigImplDev<std::string>(this, name, default_value, checker, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<std::list<std::string> >::type &default_index, const std::list<std::string> &possibilities, bool restart_when_changed)
{
    ConfigImplDev<std::list<std::string> > *impl = new ConfigImplDev<std::list<std::string> >(this, name, default_index, possibilities, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<bool>::type &default_value, bool restart_when_changed)
{
    ConfigImplDev<bool> *impl = new ConfigImplDev<bool>(this, name, default_value, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

ConfigImpl *RovizItemBaseDev::getConfigImpl(const std::string &name, const typename ConfigStorageType<FilePath>::type &default_value, FilePath::Mode file_mode, const std::string &filter, bool restart_when_changed)
{
    ConfigImplDev<FilePath> *impl = new ConfigImplDev<FilePath>(this, name, default_value, file_mode, filter, restart_when_changed);

    _this->initConfigImpl(impl);

    return impl;
}

void RovizItemBaseDev::start()
{
}

INSTANTIATE_SPARSE

INSTANTIATE_ROVIZ_BASE
