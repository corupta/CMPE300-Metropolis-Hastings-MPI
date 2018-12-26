//
// Created by CorupTa on 2018-12-25.
// Original image scores 1 and noisy image scores 0, if an image worse than noisy is supplied the score is negative.
//
// Score = (MATCHED PIXELS OF CUSTOM OUTPUT - MATCHED PIXELS OF NOISY IMAGE) / UNMATCHED PIXELS OF NOISY IMAGE
// (matched means matched with the original image, whereas unmatched means not matched with the original image)
//

"use strict";

const path = require('path');
const fs = require('fs');
const readline = require('readline');

if (process.argv.length !== 5) {
  throw new Error(
    `
      Please run the program as,
      "node pixel_comparator.js <original_file> <noisy_file> <output_file>"
    `
  );
}

class Queue {
  constructor() {
    this.top = {
      data: null,
      next: null
    }
    this.tail = this.top;
    this.done = false;
    this.eof = false;
  }
  push(data) {
    if (data) {
      this.tail.data = data;
      this.tail.next = {
        data: null,
        next: null
      };
      this.tail = this.tail.next;
    } else {
      this.eof = true;
      if(!this.top.data) {
        this.done = true;
      }
    }
  }
  front() {
    return this.top.data;
  }
  pop() {
    let data = this.top.data;
    if (data) {
      this.top = this.top.next;
      if (this.top.next === null && this.eof) {
        this.done = true;
      }
    }
    return data;
  }
}

const fileKeys = ['original', 'noisy', 'output'];
const fileTypes = fileKeys
  .reduce((acc, key) => ({ ...acc, [key]: key }), {});

const totBytes = {};

const streams = {};

const queues = {};

let worstScore = 0;
let bestScore = 0;
let currentScore = 0;

const fileReaderCreator = (filename, key) => fs.stat(filename, (err, stats) => {
  if (err) throw err;
  totBytes[key] = stats.size;
  queues[key] = new Queue();
  streams[key] = fs.createReadStream(
    path.resolve(filename)
  )
  readline.createInterface({
    input: streams[key],
    terminal: false,
    crlfDelay: Infinity
  }).on('line', (line) => {
    queues[key].push(line);
  }).on('close', () => {
    queues[key].push(null);
  });
})

const readers = process.argv.slice(2).map(
  (filename, i) => ({ 
    [fileKeys[i]]: fileReaderCreator(filename, fileKeys[i]) 
  })
).reduce((acc, x) => ({ ...acc, ...x }), {});

const clock = 4; // 4ms
let elapsed = 0;
const statusReporter = () =>
  console.log(
    fileKeys.reduce(
      (acc, key) => 
        `${acc}${key}: ${(streams[key].bytesRead / totBytes[key] * 100).toFixed(2).padStart(8)}%\t`,
      ''
    ),
    `CurrentScore: ${((currentScore - worstScore) / Math.max(1, bestScore - worstScore) * 100).toFixed(3).padStart(8)}%`,
    `\tElapsedTime: ${((elapsed += clock) / 1000).toFixed(3)} seconds...`
  );

let statusReportInterval = setInterval(statusReporter, clock);

const calculateClock = 8; // 8ms
const calculator = () => {
  if (!fileKeys
    .reduce((acc, key) => acc && (queues[key] || {}).done, true)) {
    if (fileKeys
      .reduce((acc, key) => acc && (queues[key] || { front: () => false }).front(), true)) {
      const lines = fileKeys
        .reduce((acc, key) => ({ ...acc, [key]: queues[key].pop().split(/\s+/)
        .filter((num) => num)
        .map((num) => parseInt(num, 10)) }), {});
      for (let i = 0; i < lines.original.length; ++i) {
        ++bestScore;
        worstScore += lines.noisy[i] === lines.original[i] ? 1 : 0;
        currentScore += lines.output[i] === lines.original[i] ? 1 : 0;
      }
      calculator();
    } else {
    setTimeout(calculator, calculateClock);   
    }
  } else {
    clearInterval(statusReportInterval);
    statusReporter();
  }
}

setTimeout(calculator, calculateClock);
