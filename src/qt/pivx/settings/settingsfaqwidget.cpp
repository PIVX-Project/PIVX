#include "qt/pivx/settings/settingsfaqwidget.h"
#include "qt/pivx/settings/forms/ui_settingsfaqwidget.h"
#include <QScrollBar>
#include "QFile"

SettingsFaqWidget::SettingsFaqWidget(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsFaqWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    ui->container->setProperty("cssClass", "container-welcome");

    ui->labelTitle->setProperty("cssClass", "text-title-faq");
    ui->labelTitle->setText("Frequently Asked Questions");

    ui->labelWebLink->setProperty("cssClass", "text-content-white");
    ui->labelWebLink->setText("You can read more here");


    // Content

    ui->labelNumber1->setProperty("cssClass", "container-number-faq");
    ui->labelNumber1->setText("1");
    ui->labelNumber2->setProperty("cssClass", "container-number-faq");
    ui->labelNumber2->setText("2");
    ui->labelNumber3->setProperty("cssClass", "container-number-faq");
    ui->labelNumber3->setText("3");
    ui->labelNumber4->setProperty("cssClass", "container-number-faq");
    ui->labelNumber4->setText("4");
    ui->labelNumber5->setProperty("cssClass", "container-number-faq");
    ui->labelNumber5->setText("5");
    ui->labelNumber6->setProperty("cssClass", "container-number-faq");
    ui->labelNumber6->setText("6");
    ui->labelNumber7->setProperty("cssClass", "container-number-faq");
    ui->labelNumber7->setText("7");
    ui->labelNumber8->setProperty("cssClass", "container-number-faq");
    ui->labelNumber8->setText("8");



    ui->labelSubtitle1->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle1->setText("What is PIVX");
    ui->labelSubtitle2->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle2->setText("Why my PIV are unspendable");
    ui->labelSubtitle3->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle3->setText("PIVX privacy? What is zPIV, zerocoin.");
    ui->labelSubtitle4->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle4->setText("Why my zPIV are unspendable");
    ui->labelSubtitle5->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle5->setText("Why my wallet convert my balance into zPIV automatically?");
    ui->labelSubtitle6->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle6->setText("How do i receive PIV/zPIV?");
    ui->labelSubtitle7->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle7->setText("How do i stake PIV/zPIV?");
    ui->labelSubtitle8->setProperty("cssClass", "text-subtitle-faq");
    ui->labelSubtitle8->setText("Where i should go if i need support?");

    ui->labelContent1->setProperty("cssClass", "text-content-faq");

    ui->labelContent2->setProperty("cssClass", "text-content-faq");
    ui->labelContent3->setProperty("cssClass", "text-content-faq");
    ui->labelContent3->setOpenExternalLinks( true );
    ui->labelContent4->setProperty("cssClass", "text-content-faq");
    ui->labelContent5->setProperty("cssClass", "text-content-faq");
    ui->labelContent5->setOpenExternalLinks( true );
    ui->labelContent6->setProperty("cssClass", "text-content-faq");
    ui->labelContent7->setProperty("cssClass", "text-content-faq");
    ui->labelContent8->setProperty("cssClass", "text-content-faq");
    ui->labelContent8->setOpenExternalLinks( true );

    // Exit button

    ui->pushButtonExit->setText("Exit");
    ui->pushButtonExit->setProperty("cssClass", "btn-faq-exit");

    // Web Link

    ui->pushButtonWebLink->setText("https://PIVX.org/zpiv");
    ui->pushButtonWebLink->setProperty("cssClass", "btn-faq-web");


    // Questions buttons

    ui->containerButtons->setProperty("cssClass", "container-faq-buttons");

    ui->pushButtonFaq1->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq2->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq3->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq4->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq5->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq6->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq7->setProperty("cssClass", "btn-faq-options");
    ui->pushButtonFaq8->setProperty("cssClass", "btn-faq-options");

    // Buttons

    connect(ui->pushButtonExit, SIGNAL(clicked()), this, SLOT(close()));

    connect(ui->pushButtonFaq1, SIGNAL(clicked()), this, SLOT(onFaq1Clicked()));
    connect(ui->pushButtonFaq2, SIGNAL(clicked()), this, SLOT(onFaq2Clicked()));
    connect(ui->pushButtonFaq3, SIGNAL(clicked()), this, SLOT(onFaq3Clicked()));
    connect(ui->pushButtonFaq4, SIGNAL(clicked()), this, SLOT(onFaq4Clicked()));
    connect(ui->pushButtonFaq5, SIGNAL(clicked()), this, SLOT(onFaq5Clicked()));
    connect(ui->pushButtonFaq6, SIGNAL(clicked()), this, SLOT(onFaq6Clicked()));
    connect(ui->pushButtonFaq7, SIGNAL(clicked()), this, SLOT(onFaq7Clicked()));
    connect(ui->pushButtonFaq8, SIGNAL(clicked()), this, SLOT(onFaq8Clicked()));

}

void SettingsFaqWidget::onFaq1Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq1->y());

}

void SettingsFaqWidget::onFaq2Clicked(){
   ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq2->y());
}

void SettingsFaqWidget::onFaq3Clicked(){
   ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq3->y());
}

void SettingsFaqWidget::onFaq4Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq4->y());
}

void SettingsFaqWidget::onFaq5Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq5->y());
}

void SettingsFaqWidget::onFaq6Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq6->y());
}

void SettingsFaqWidget::onFaq7Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq7->y());
}

void SettingsFaqWidget::onFaq8Clicked(){
    ui->scrollAreaFaq->verticalScrollBar()->setValue(ui->widgetFaq8->y());
}

SettingsFaqWidget::~SettingsFaqWidget()
{
    delete ui;
}
