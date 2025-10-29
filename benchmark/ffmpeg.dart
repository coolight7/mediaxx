// Create a new benchmark by extending BenchmarkBase
import 'package:benchmark_harness/benchmark_harness.dart';

class FFmpegBenchmark extends BenchmarkBase {
  const FFmpegBenchmark() : super('ffmpeg');

  static void main() {
    const FFmpegBenchmark().report();
  }

  // The benchmark code.
  @override
  void run() {}

  // Not measured setup code executed prior to the benchmark runs.
  @override
  void setup() {}

  // Not measured teardown code executed after the benchmark runs.
  @override
  void teardown() {}

  // To opt into the reporting the time per run() instead of per 10 run() calls.
  @override
  void exercise() {
    for (var i = 0; i < 10000; i++) {
      run();
    }
  }
}

void main() {
  // Run TemplateBenchmark
  FFmpegBenchmark.main();
}
