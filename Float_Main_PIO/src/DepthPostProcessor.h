#ifndef DEPTH_POST_PROCESSOR_H
#define DEPTH_POST_PROCESSOR_H

class DepthPostProcessor
{
public:
    struct Config
    {
        float jitterBandM;
        float smoothingAlpha;
    };

    DepthPostProcessor();

    void reset();
    void setConfig(const Config &config);
    float apply(float filteredDepthM);

private:
    Config config_;
    float lastOutputM_;
    bool ready_;
};

#endif
