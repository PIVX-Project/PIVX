//
// Created by furszy on 2019-05-07.
//

#ifndef PIVX_CORE_NEW_GUI_PRUNNABLE_H
#define PIVX_CORE_NEW_GUI_PRUNNABLE_H

class Runnable {
public:
    virtual void run(int type) = 0;
    virtual void onError(int type, QString error) = 0;
};

#endif //PIVX_CORE_NEW_GUI_PRUNNABLE_H
