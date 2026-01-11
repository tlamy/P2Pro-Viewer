#ifndef SCALER_HPP
#define SCALER_HPP

class Scaler {
public:
    Scaler(int baseWidth, int baseHeight);

    void getScaledSize(int inputW, int inputH, int &outputW, int &outputH) const;

private:
    int baseW;
    int baseH;
    double k;
};

#endif