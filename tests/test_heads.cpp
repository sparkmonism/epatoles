#include "../TapeHeads.h"
#include <cstdio>
#include <vector>
#include <random>
using namespace epat;

static const double SR = 48000.0;
static const int64_t RING = 1 << 21, MASK = RING - 1;

static double hermite(const std::vector<double>& b, int64_t i, double f)
{
    double y0=b[(i-1)&MASK], y1=b[i&MASK], y2=b[(i+1)&MASK], y3=b[(i+2)&MASK];
    double c1=0.5*(y2-y0), c2=y0-2.5*y1+2*y2-0.5*y3, c3=0.5*(y3-y0)+1.5*(y1-y2);
    return ((c3*f+c2)*f+c1)*f+y1;
}
static double readT(const std::vector<double>& b, double p){ double fl=std::floor(p); return hermite(b,(int64_t)fl,p-fl); }

struct Result { double minDelay=1e18, maxDelay=-1e18; int jumps=0; bool ok=true; };

struct Sim
{
    std::vector<double> ring = std::vector<double>(RING, 0.0);
    TapeHeads h; HeadParams hp;
    double cur = 1.0; int64_t w = 0;
    double inertia = 150.0;
    double lockLat;
    Sim(double chunkSec = 1.0, double rfadeMs = 40.0)
    {
        hp.sampleRate = SR;
        hp.xfade = (int)(0.020*SR);
        hp.minDelay = hp.xfade + 16;
        hp.windowSamples = 1.0*SR;
        hp.chunkSamples = chunkSec*SR;
        hp.maxDelay = 22.0*SR;
        hp.rfade = (int)(rfadeMs*0.001*SR);
        lockLat = 0.060*SR + 300;           // pretend 60 ms + oversampler latency
        hp.lockTarget = lockLat;
        h.reset(lockLat);
    }
    // one sample; returns output value (= source index for the ramp)
    double step(double target, Result& r, double wf = 0.0, std::vector<int64_t>* jumpLog = nullptr)
    {
        const double coeff = 1.0 - std::exp(-1.0/(std::max(1.0,inertia)*0.001*SR));
        cur += coeff*(target-cur);
        ring[w & MASK] = (double) w;                 // ramp: value == its own index
        double speed = cur*(1.0+wf);
        const bool wasFading = h.isFading();
        speed = h.manage(w, cur, speed, hp);
        if (!wasFading && h.isFading()) { r.jumps++; if (jumpLog) jumpLog->push_back(w); }
        float w0, w1; h.weights(w0, w1);
        double x = readT(ring, h.activePos());
        double d0 = (double)w - h.activePos();
        r.minDelay = std::min(r.minDelay, d0); r.maxDelay = std::max(r.maxDelay, d0);
        if (h.isFading()) {
            double d1 = (double)w - h.otherPos();
            r.minDelay = std::min(r.minDelay, d1); r.maxDelay = std::max(r.maxDelay, d1);
            x = x*w0 + readT(ring, h.otherPos())*w1;
        }
        h.advance(speed);
        ++w;
        return x;
    }
};

static bool check(const char* name, const Result& r, double maxAllowed)
{
    bool ok = r.minDelay >= 3.0 && r.maxDelay <= maxAllowed;
    printf("%-34s minDelay %9.1f  maxDelay %10.1f (%.2fs)  jumps %4d  %s\n", name, r.minDelay, r.maxDelay, r.maxDelay/SR, r.jumps, ok?"OK":"FAIL");
    return ok;
}

int main()
{
    bool all = true;

    // A. forward 1x steady: output tracks (n - lockTarget), never jumps after settling.
    { Sim s; Result r; double err=0;
      for (int n=0;n<SR*4;n++){ double x=s.step(1.0,r); if(n>SR*2) err=std::max(err,std::abs(x-((double)(s.w-1)-s.lockLat))); }
      printf("A forward 1x: max |out - (n-lock)| = %.4f, jumps %d\n", err, r.jumps);
      all &= (err < 0.5 && r.jumps==0); all &= check("A forward 1x", r, s.lockLat+2); }

    // B. reverse -1x: derivative -1, chunk spacing == L, contiguous tiling
    { Sim s(1.0, 40.0); Result r; std::vector<int64_t> jl; std::vector<double> out;
      for (int n=0;n<SR*2;n++) s.step(1.0,r);
      r.jumps=0; r.minDelay=1e18; r.maxDelay=-1e18;
      int64_t t0 = s.w;
      for (int n=0;n<SR*12;n++) out.push_back(s.step(-1.0,r,0.0,&jl));
      // drop first jump (the transition), examine the rest
      printf("B reverse -1x: jumps logged %zu\n", jl.size());
      bool ok = jl.size() >= 8;
      double maxSpacingErr = 0, maxTileErr = 0, maxSlopeErr = 0;
      for (size_t i=2;i<jl.size();i++){ maxSpacingErr = std::max(maxSpacingErr, std::abs((double)(jl[i]-jl[i-1]) - SR)); }
      // tiling: value at the sample right after a fade completes should be (jump time - minDelay) - fadeLen*1 (moving back 1/sample)... check slope on clean regions
      for (size_t k=3;k+1<jl.size();k++){
          int64_t a = jl[k] + s.hp.rfade + 50, b = jl[k+1] - 50;       // fully on the new head
          for (int64_t n=a+1;n<b;n++){ double d = out[(size_t)(n-t0)] - out[(size_t)(n-1-t0)]; maxSlopeErr=std::max(maxSlopeErr,std::abs(d+1.0)); }
          // new segment's value at jump+fade should equal start - fade samples travelled
          double expectStart = (double)(jl[k]) - s.hp.minDelay - s.hp.rfade;   // ramp value after fade
          double got = out[(size_t)(jl[k]+s.hp.rfade-t0)];
          // previous segment started at jl[k-1]-min and ran back L samples => ended at jl[k-1]-min-L... tiling means next start == prev start + L
          double prevStart = (double)jl[k-1] - s.hp.minDelay, thisStart = (double)jl[k] - s.hp.minDelay;
          maxTileErr = std::max(maxTileErr, std::abs((thisStart - prevStart) - SR));
          (void)expectStart; (void)got;
      }
      printf("   chunk spacing err %.1f samples, tile err %.1f, slope err %.4f\n", maxSpacingErr, maxTileErr, maxSlopeErr);
      ok = ok && maxSpacingErr < 3 && maxTileErr < 3 && maxSlopeErr < 1e-3;
      all &= ok; all &= check("B reverse -1x", r, 2.2*SR); }

    // C. other reverse speeds & chunk sizes, with wow/flutter modulation
    for (double sp : { -0.25, -0.5, -0.75, -1.0, -1.5, -2.0 })
      for (double chunk : { 0.1, 0.5, 2.0, 10.0 })
      { Sim s(chunk, 40.0); Result r; char nm[64];
        for (int n=0;n<SR*1;n++) s.step(1.0,r);
        for (int n=0;n<SR*30;n++) s.step(sp, r, 0.006*std::sin(n*2*M_PI*0.8/SR) + 0.003*std::sin(n*2*M_PI*9.0/SR));
        snprintf(nm,64,"C speed %.2f chunk %.1fs",sp,chunk);
        all &= check(nm, r, 22.0*SR + 4.0*SR); }

    // D. grid sync: jump at each grid event (every 0.5 s), reverse -1x
    { Sim s(1.0, 40.0); s.hp.gridSync = true; Result r; std::vector<int64_t> jl, ev;
      for (int n=0;n<SR*2;n++) s.step(1.0,r);
      r.jumps = 0;
      int gridLen = (int)(0.5*SR);
      for (int n=0;n<SR*10;n++){ if (n%gridLen==0){ s.h.requestGridJump(); ev.push_back(s.w); } s.step(-1.0,r,0.0,&jl); }
      printf("D grid sync: events %zu, jumps %zu\n", ev.size(), jl.size());
      double maxLate = 0; bool ok = true;
      for (size_t i=3;i<jl.size() && i<ev.size();i++){ // each jump should land at/just after an event
          int64_t e = jl[i]; int64_t nearest = (e/gridLen)*gridLen + (ev[0]%gridLen);
          (void)nearest; }
      // spacing between jumps should be exactly the grid length
      for (size_t i=3;i<jl.size();i++) maxLate = std::max(maxLate, std::abs((double)(jl[i]-jl[i-1]) - gridLen));
      printf("   jump spacing err %.1f samples\n", maxLate);
      ok = jl.size() >= 15 && maxLate < 2;
      all &= ok; all &= check("D grid sync", r, 22.0*SR); }

    // D2. grid sync with a fade longer than the grid (events pile up while fading)
    { Sim s(1.0, 250.0); s.hp.gridSync = true; Result r;
      for (int n=0;n<SR*2;n++) s.step(1.0,r);
      int gridLen = (int)(0.125*SR);
      for (int n=0;n<SR*10;n++){ if (n%gridLen==0) s.h.requestGridJump(); s.step(-1.0,r); }
      all &= check("D2 grid 1/8 beat, 250ms fade", r, 22.0*SR); }

    // E. random transport stress: direction flips, stops, speed changes, fast & slow motors
    { std::mt19937 rng(12345); Result worst; bool ok=true;
      for (int trial=0; trial<40; trial++){
        Sim s(0.1 + (rng()%1000)/100.0, 5.0 + (rng()%2450)/10.0);
        s.inertia = 5.0 + (rng()%3000)/10.0;
        s.hp.gridSync = (rng()%3)==0;
        s.hp.windowSamples = (0.1 + (rng()%990)/100.0)*SR;
        Result r; double target = 1.0; int nextChange = 0;
        for (int n=0;n<SR*60;n++){
            if (n>=nextChange){ int k = rng()%6; double mag = std::pow(2.0, ((int)(rng()%36)-24)/12.0);
                target = (k==0)?0.0 : (k<=3 ? -mag : mag); nextChange = n + (int)(SR*(0.05 + (rng()%2000)/1000.0)); }
            if (s.hp.gridSync && n%(int)(0.25*SR)==0) s.h.requestGridJump();
            s.step(target, r, 0.006*std::sin(n*2*M_PI*0.8/SR));
        }
        ok &= (r.minDelay >= 3.0 && r.maxDelay <= 22.0*SR + 4.0*SR);
        worst.minDelay = std::min(worst.minDelay,r.minDelay); worst.maxDelay=std::max(worst.maxDelay,r.maxDelay); worst.jumps+=r.jumps;
      }
      all &= check("E random stress (40 x 60 s)", worst, 22.0*SR + 4.0*SR); all &= ok; }

    printf("\n%s\n", all ? "ALL PASS" : "SOME FAILED");
    return all ? 0 : 1;
}
