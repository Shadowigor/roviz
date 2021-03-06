#ifndef GAUSSIANBLUR_ITEM_H
#define GAUSSIANBLUR_ITEM_H

#include "core/roviz_item.h"
#include "streams/image_m.h"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"

/**
 * @brief Blurs the image with a gaussian blur
 *
 * \ingroup robot_plugins
 */
class GaussianBlurItem : public RovizItem
{
#if ROVIZ_BACKEND == ROVIZ_BACKEND_Dev
    Q_OBJECT
#endif

public:
    ROVIZ_INVOKABLE GaussianBlurItem();
    ~GaussianBlurItem();

protected:
    void thread(void) override;

private:
    Input<Image> input;
    Output<Image> output;
    Trim trim_sigma, trim_ksize;
};

#endif // GAUSSIANBLUR_ITEM_H
