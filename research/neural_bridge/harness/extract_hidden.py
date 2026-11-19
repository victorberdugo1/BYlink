import argparse
import json
from pathlib import Path

import numpy as np
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--model", required=True)
    p.add_argument("--revision", required=True)
    p.add_argument("--anchors", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--depths", default="0.25,0.5,0.75,1.0")
    p.add_argument("--max-length", type=int, default=512)
    a = p.parse_args()
    anchors = json.loads(Path(a.anchors).read_text())
    tok = AutoTokenizer.from_pretrained(a.model, revision=a.revision)
    model = AutoModelForCausalLM.from_pretrained(a.model, revision=a.revision, torch_dtype=torch.bfloat16)
    model.eval()
    n_layers = model.config.num_hidden_layers
    depths = [float(d) for d in a.depths.split(",")]
    layer_ids = {d: max(1, round(d * n_layers)) for d in depths}
    feats = {d: [] for d in depths}
    token_counts = []
    with torch.no_grad():
        for item in anchors:
            enc = tok(item["text"], return_tensors="pt", truncation=True, max_length=a.max_length)
            token_counts.append(int(enc["input_ids"].shape[1]))
            hidden = model(**enc, output_hidden_states=True).hidden_states
            for d, layer in layer_ids.items():
                feats[d].append(hidden[layer][0].float().mean(0).numpy())
    np.savez(
        a.out,
        **{f"depth_{d}": np.stack(v) for d, v in feats.items()},
        token_counts=np.array(token_counts),
        anchor_ids=np.array([x["id"] for x in anchors]),
        meta=json.dumps({"model": a.model, "revision": a.revision, "n_layers": n_layers, "layer_ids": {str(k): v for k, v in layer_ids.items()}}),
    )


if __name__ == "__main__":
    main()
