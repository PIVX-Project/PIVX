#include "qt/pivx/dashboardwidget.h"
#include "qt/pivx/forms/ui_dashboardwidget.h"
#include "qt/pivx/sendconfirmdialog.h"
#include "qt/pivx/txrow.h"
#include "qt/pivx/qtutils.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "qt/pivx/settings/settingsfaqwidget.h"
#include <QFile>
#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QModelIndex>
#include <QObject>
#include <QPaintEngine>
#include <QGraphicsDropShadowEffect>
#include <iostream>
#include <cstdlib>

#include <QMap>

// Chart
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QGraphicsLayout>

#define DECORATION_SIZE 65
#define NUM_ITEMS 3

#include "moc_dashboardwidget.cpp"

DashboardWidget::DashboardWidget(PIVXGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::DashboardWidget)
{
    ui->setupUi(this);

    txHolder = new TxViewHolder(isLightTheme());
    txViewDelegate = new FurAbstractListItemDelegate(
                DECORATION_SIZE,
                txHolder,
                this
    );

    // Load css.
    this->setStyleSheet(parent->styleSheet());
    this->setContentsMargins(0,0,0,0);

    setProperty("cssClass", "container");

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(0,0,0,0);
    ui->right->setProperty("cssClass", "container-right");
    ui->right->setContentsMargins(20,20,20,20);

    // Title
    ui->labelTitle->setProperty("cssClass", "text-title-screen");

    ui->labelTitle2->setText(tr("Staking Rewards"));
    ui->labelTitle2->setProperty("cssClass", "text-title-screen");

    /* Subtitle */

    ui->labelSubtitle->setText(tr("You can view your account's history"));
    ui->labelSubtitle->setProperty("cssClass", "text-subtitle");


    // Staking Information
    ui->labelMessage->setText(tr("Amount of PIV and zPIV staked."));
    ui->labelMessage->setProperty("cssClass", "text-subtitle");
    ui->labelSquarePiv->setProperty("cssClass", "square-chart-piv");
    ui->labelSquarezPiv->setProperty("cssClass", "square-chart-zpiv");
    ui->labelPiv->setProperty("cssClass", "text-chart-piv");
    ui->labelZpiv->setProperty("cssClass", "text-chart-zpiv");

    // Staking Amount
    QFont fontBold;
    fontBold.setWeight(QFont::Bold);

    ui->labelChart->setProperty("cssClass", "legend-chart");

    ui->labelAmountZpiv->setText("0 zPIV");
    ui->labelAmountPiv->setText("0 PIV");
    ui->labelAmountPiv->setProperty("cssClass", "text-stake-piv-disable");
    ui->labelAmountZpiv->setProperty("cssClass", "text-stake-zpiv-disable");


    // Chart
    //ui->verticalWidgetChart->setProperty("cssClass", "container-chart");

    ui->pushButtonHour->setProperty("cssClass", "btn-check-time");
    ui->pushButtonDay->setProperty("cssClass", "btn-check-time");
    ui->pushButtonWeek->setProperty("cssClass", "btn-check-time");
    ui->pushButtonMonth->setProperty("cssClass", "btn-check-time");
    ui->pushButtonYear->setProperty("cssClass", "btn-check-time");
    ui->pushButtonYear->setChecked(true);


    // Sort Transactions
    ui->comboBoxSort->setProperty("cssClass", "btn-combo");
    ui->comboBoxSort->setEditable(true);
    SortEdit* lineEdit = new SortEdit(ui->comboBoxSort);
    lineEdit->setReadOnly(true);
    lineEdit->setAlignment(Qt::AlignRight);
    ui->comboBoxSort->setLineEdit(lineEdit);
    ui->comboBoxSort->setStyleSheet("selection-background-color:transparent; selection-color:transparent;");
    connect(lineEdit, SIGNAL(Mouse_Pressed()), this, SLOT(onSortTxPressed()));
    ui->comboBoxSort->setView(new QListView());
    ui->comboBoxSort->addItem("Date");
    ui->comboBoxSort->addItem("Amount");
    //ui->comboBoxSort->addItem("Sent");
    //ui->comboBoxSort->addItem("Received");
    connect(ui->comboBoxSort, SIGNAL(currentIndexChanged(const QString&)), this,SLOT(onSortChanged(const QString&)));

    // transactions
    ui->listTransactions->setProperty("cssClass", "container");
    ui->listTransactions->setItemDelegate(txViewDelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listTransactions->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Sync Warning
    ui->layoutWarning->setVisible(true);
    ui->lblWarning->setText(tr("Please wait until the wallet is fully synced to see your correct balance"));
    ui->lblWarning->setProperty("cssClass", "text-warning");
    ui->imgWarning->setProperty("cssClass", "ic-warning");

    //Empty List
    ui->emptyContainer->setVisible(false);
    ui->pushImgEmpty->setProperty("cssClass", "img-empty-transactions");

    ui->labelEmpty->setText(tr("No transactions yet"));
    ui->labelEmpty->setProperty("cssClass", "text-empty");


    ui->chartContainer->setProperty("cssClass", "container-chart");

    ui->pushImgEmptyChart->setProperty("cssClass", "img-empty-staking-on");

    ui->btnHowTo->setText(tr("How to get PIV or zPIV"));
    ui->btnHowTo->setProperty("cssClass", "btn-secundary");

    // Staking off
    //ui->pushImgEmptyChart->setProperty("cssClass", "img-empty-staking-off");
    //ui->labelEmptyChart->setText("Staking off");

    ui->labelEmptyChart->setText(tr("Staking off"));
    ui->labelEmptyChart->setProperty("cssClass", "text-empty");

    ui->labelMessageEmpty->setText(tr("You can activate and deactivate the Staking mode in the status bar at the top right of the wallet"));
    ui->labelMessageEmpty->setProperty("cssClass", "text-subtitle");

    // Chart State
    ui->layoutChart->setVisible(false);
    ui->emptyContainerChart->setVisible(true);
    setShadow(ui->layoutShadow);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));
}

void DashboardWidget::handleTransactionClicked(const QModelIndex &index){

    ui->listTransactions->setCurrentIndex(index);

    QModelIndex rIndex = filter->mapToSource(index);

    window->showHide(true);
    TxDetailDialog *dialog = new TxDetailDialog(window, false);
    dialog->setData(walletModel, rIndex);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 17);

    // Back to regular status
    ui->listTransactions->scrollTo(index);
    ui->listTransactions->clearSelection();
    ui->listTransactions->setFocus();

}

void DashboardWidget::initChart() {
    set0 = new QBarSet("PIV");
    set1 = new QBarSet("zPIV");
    set0->setColor(QColor(92,75,125));
    set1->setColor(QColor(176,136,255));

    chart = new QChart();
}

const char * monthsNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void DashboardWidget::loadChart(){

    int size = stakesFilter->rowCount();
    if (size > 0) {
        ui->layoutChart->setVisible(true);
        ui->emptyContainerChart->setVisible(false);
        initChart();

        // pair PIV, zPIV
        QMap<int, std::pair<qint64, qint64>> amountByMonths;
        bool hasZpivStakes = false;

        // get all of the stakes
        for (int i = 0; i < size; ++i) {
            QModelIndex modelIndex = stakesFilter->index(i, TransactionTableModel::ToAddress);
            qint64 amount = llabs(modelIndex.data(TransactionTableModel::AmountRole).toLongLong());
            QDateTime datetime = modelIndex.data(TransactionTableModel::DateRole).toDateTime();
            bool isPiv = modelIndex.data(TransactionTableModel::TypeRole).toInt() != TransactionRecord::StakeZPIV;
            int month = datetime.date().month();
            if (amountByMonths.contains(month)) {
                if (isPiv) {
                    amountByMonths[month].first += amount;
                } else
                    amountByMonths[month].second += amount;
            } else {
                if (isPiv) {
                    amountByMonths[month] = std::make_pair(amount, 0);
                } else {
                    amountByMonths[month] = std::make_pair(0, amount);
                    hasZpivStakes = true;
                }
            }

        }

        QStringList months;
        qreal maxValue = 0;
        qint64 totalPiv = 0;
        qint64 totalZpiv = 0;
        for (int j = 12; j > 0; j--) {
            qreal piv = 0;
            qreal zpiv = 0;
            if (amountByMonths.contains(j)) {
                std::pair<qint64, qint64> pair = amountByMonths[j];
                piv = (pair.first != 0) ? pair.first / 100000000 : 0;
                zpiv = (pair.second != 0) ? pair.second / 100000000 : 0;
                totalPiv += pair.first;
                totalZpiv += pair.second;
            }
            months << monthsNames[j - 1];
            set0->append(piv);
            set1->append(zpiv);

            int max = std::max(piv, zpiv);
            if (max > maxValue) {
                maxValue = max;
            }
        }

        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

        if (totalPiv > 0 || totalZpiv > 0) {
            ui->labelAmountPiv->setProperty("cssClass", "text-stake-piv");
            ui->labelAmountZpiv->setProperty("cssClass", "text-stake-zpiv");
        } else {
            ui->labelAmountPiv->setProperty("cssClass", "text-stake-piv-disable");
            ui->labelAmountZpiv->setProperty("cssClass", "text-stake-zpiv-disable");
        }
        forceUpdateStyle({ui->labelAmountPiv, ui->labelAmountZpiv});
        ui->labelAmountPiv->setText(GUIUtil::formatBalance(totalPiv, nDisplayUnit));
        ui->labelAmountZpiv->setText(GUIUtil::formatBalance(totalZpiv, nDisplayUnit, true));

        QBarSeries *series = new QtCharts::QBarSeries();
        series->append(set0);
        if(hasZpivStakes)
            series->append(set1);

        // bar width
        series->setBarWidth(0.8);

        chart->addSeries(series);
        chart->setAnimationOptions(QChart::SeriesAnimations);

        axisX = new QBarCategoryAxis();
        axisX->append(months);
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        axisY = new QValueAxis();
        axisY->setRange(0,maxValue);
        chart->addAxis(axisY, Qt::AlignRight);
        series->attachAxis(axisY);

        // Legend
        chart->legend()->setVisible(false);
        chart->legend()->setAlignment(Qt::AlignTop);

        QChartView *chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);

        QVBoxLayout *baseScreensContainer = new QVBoxLayout(this);
        baseScreensContainer->setMargin(0);
        ui->chartContainer->setLayout(baseScreensContainer);

        ui->chartContainer->layout()->addWidget(chartView);
        ui->chartContainer->setProperty("cssClass", "container-chart");

        // Set colors
        changeChartColors();

        // Chart margin removed.
        chart->layout()->setContentsMargins(0, 0, 0, 0);
        chart->setBackgroundRoundness(0);
    } else {
        ui->layoutChart->setVisible(false);
        ui->emptyContainerChart->setVisible(true);
    }
}

void DashboardWidget::changeChartColors(){
    QColor gridLineColorX;
    QColor linePenColorY;
    QColor backgroundColor;
    QColor gridY;
    if(isLightTheme()){
        gridLineColorX = QColor(255,255,255);
        linePenColorY = gridLineColorX;
        backgroundColor = linePenColorY;
    }else{
        gridY = QColor("#40ffffff");
        axisY->setGridLineColor(gridY);
        gridLineColorX = QColor(15,11,22);
        linePenColorY =  gridLineColorX;
        backgroundColor = linePenColorY;
    }

    axisX->setGridLineColor(gridLineColorX);
    axisY->setLinePenColor(linePenColorY);
    chart->setBackgroundBrush(QBrush(backgroundColor));
    set0->setBorderColor(gridLineColorX);
    set1->setBorderColor(gridLineColorX);
}

void DashboardWidget::loadWalletModel(){
    if (walletModel && walletModel->getOptionsModel()) {
        txModel = walletModel->getTransactionTableModel();
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setDynamicSortFilter(true);
        filter->setSortCaseSensitivity(Qt::CaseInsensitive);
        filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        filter->setSortRole(Qt::EditRole);
        filter->setSourceModel(txModel);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);
        txHolder->setFilter(filter);
        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        if(txModel->size() == 0){
            ui->emptyContainer->setVisible(true);
            ui->listTransactions->setVisible(false);
            connect(txModel, SIGNAL(firstTxArrived()), this, SLOT(showList()));
            connect(ui->pushImgEmpty, SIGNAL(clicked()), this, SLOT(openFAQ()));
            connect(ui->btnHowTo, SIGNAL(clicked()), this, SLOT(openFAQ()));
        }

        // chart filter
        stakesFilter = new TransactionFilterProxy();
        stakesFilter->setSourceModel(txModel);
        stakesFilter->sort(TransactionTableModel::Date, Qt::AscendingOrder);
        stakesFilter->setOnlyStakes(true);
        loadChart();
    }
    // update the display unit, to not use the default ("PIV")
    updateDisplayUnit();
}

void DashboardWidget::openFAQ(){
    showHideOp(true);
    SettingsFaqWidget* dialog = new SettingsFaqWidget(window);
    openDialogWithOpaqueBackgroundFullScreen(dialog, window);
}

void DashboardWidget::showList(){
    ui->emptyContainer->setVisible(false);
    ui->listTransactions->setVisible(true);
}

void DashboardWidget::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();

        txHolder->setDisplayUnit(nDisplayUnit);
        ui->listTransactions->update();
    }
}

void DashboardWidget::onSortTxPressed(){
    ui->comboBoxSort->showPopup();
}

void DashboardWidget::onSortChanged(const QString& value){
    if(!value.isNull()) {
        if (value == "Amount")
            filter->sort(TransactionTableModel::Amount, Qt::DescendingOrder);
        else if (value == "Date")
            filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);
        ui->listTransactions->update();
    }
}

void DashboardWidget::walletSynced(bool sync){
    if (this->isSync != sync) {
        this->isSync = sync;
        ui->layoutWarning->setVisible(!this->isSync);
    }
}

void DashboardWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<TxViewHolder*>(this->txViewDelegate->getRowFactory())->isLightTheme = isLightTheme;
    if (stakesFilter->rowCount() > 0)
        this->changeChartColors();
}

DashboardWidget::~DashboardWidget()
{
    delete ui;
}
