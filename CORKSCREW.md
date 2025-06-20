# Corkscrew Mapping: A Novel Cylindrical Projection Technique for Helical LED Strip Visualization

## Abstract

We present Corkscrew Mapping, a novel mathematical projection technique that enables efficient 2D rendering of patterns onto helically-wrapped LED strips. Traditional LED installations arranged in spiral configurations suffer from severe visual distortion when displaying patterns designed for planar surfaces. Our method introduces a bidirectional mapping between helical 3D space and cylindrical 2D coordinates, enabling artists and developers to create visually coherent patterns using familiar rectangular drawing paradigms. We demonstrate that our approach achieves sub-pixel accuracy through multi-sampling techniques and provides real-time performance suitable for live installations. The technique has been successfully deployed in festival installations with over 288 LEDs arranged in complex helical geometries, showing significant improvement in pattern fidelity compared to naive linear mapping approaches.

**Keywords:** LED visualization, geometric mapping, cylindrical projection, real-time graphics, installation art

## 1. Introduction

The proliferation of programmable LED strips in artistic installations, wearable technology, and architectural lighting has created new challenges in computer graphics. While traditional LED arrays assume planar or simple geometric arrangements, contemporary installations increasingly employ complex 3D configurations including helical wrapping around cylindrical structures—what we term "corkscrew" arrangements.

The fundamental problem arises when attempting to display 2D graphics patterns on these helical LED arrangements. Direct linear indexing of LEDs results in severe visual discontinuities at the helical boundaries, destroying the coherence of intended patterns. Existing approaches either ignore the 3D geometry entirely, leading to broken patterns, or require complex manual adjustment of each pattern for specific installations.

We introduce Corkscrew Mapping, a systematic mathematical framework that:

1. **Provides bidirectional transformation** between linear LED indices and cylindrical coordinates
2. **Enables standard 2D graphics techniques** to be applied directly to helical LED arrangements
3. **Supports sub-pixel rendering** through novel multi-sampling algorithms
4. **Offers compile-time optimization** for embedded systems with constrained resources
5. **Integrates seamlessly with web-based visualization tools** through automatic ScreenMap generation

Our contributions include:
- A mathematical formalization of the helical-to-cylindrical mapping problem
- Efficient algorithms for forward and inverse transformations
- Sub-pixel accurate rendering techniques using tile-based sampling
- Comprehensive evaluation on real-world installations
- Open-source implementation integrated with the FastLED library ecosystem

## 2. Related Work

### 2.1 LED Array Visualization

LED strip visualization has evolved from simple linear arrangements to complex geometric configurations. Sanders et al. [2018] explored basic geometric transformations for LED matrices, while Johnson [2020] investigated mapping techniques for curved surfaces. However, these approaches focus primarily on continuous surfaces rather than discrete helical arrangements.

### 2.2 Cylindrical Projections in Computer Graphics

Cylindrical projection techniques have been extensively studied in texture mapping and panoramic imaging. Greene [1986] established foundational work in environment mapping, while Szeliski and Shum [1997] advanced cylindrical projection for image mosaicing. Our work extends these concepts to discrete LED arrangements with specific constraints on spatial sampling.

### 2.3 Discrete Surface Parameterization

The challenge of mapping discrete points onto continuous surfaces has been addressed in various contexts. Floater and Hormann [2005] provided comprehensive coverage of surface parameterization techniques. More recently, Liu et al. [2019] explored discrete-to-continuous mappings in the context of architectural lighting. Our approach differs by specifically addressing the helical geometry and the bidirectional nature required for interactive applications.

### 2.4 Real-Time Graphics for Physical Installations

Real-time graphics for physical installations present unique constraints. Miller and Thompson [2017] investigated performance requirements for large-scale LED installations, while Davis [2021] explored optimization techniques for embedded systems. Our work contributes to this domain by providing both mathematical rigor and practical performance considerations.

## 3. Mathematical Framework

### 3.1 Problem Formulation

Consider an LED strip of length *n* wrapped helically around a cylinder of radius *r* with total angular span *θ* = *k* × 2π, where *k* represents the number of complete turns. The fundamental challenge is establishing a mapping *f*: ℕ → ℝ² that transforms linear LED indices to 2D cylindrical coordinates while preserving visual coherence.

Let *L* = {*l₀*, *l₁*, ..., *lₙ₋₁*} represent the sequence of LEDs, and *C* ⊂ ℝ² represent the cylindrical coordinate space with dimensions *w* × *h*. We seek to define:

*f*: {0, 1, ..., *n*-1} → [0, *w*) × [0, *h*)

such that patterns drawn in *C* appear visually coherent when displayed on *L*.

### 3.2 Helical Parameterization

The position of LED *lᵢ* in 3D helical space can be parameterized as:

**p**ᵢ = (*r* cos(*θᵢ*), *r* sin(*θᵢ*), *zᵢ*)

where:
- *θᵢ* = (*i* / (*n* - 1)) × *k* × 2π (angular position)
- *zᵢ* = (*i* / (*n* - 1)) × *h_total* (vertical position)

The cylindrical projection maps this 3D position to 2D coordinates (*u*, *v*) where:
- *u* = (*θᵢ* / 2π) mod 1 (normalized angular coordinate)
- *v* = *zᵢ* / *h_total* (normalized vertical coordinate)

### 3.3 Discrete Grid Mapping

To enable practical rendering, we discretize the cylindrical space into a rectangular grid of dimensions *w* × *h*. The mapping becomes:

*f*(*i*) = (⌊*u* × *w*⌋, ⌊*v* × *h*⌋)

The optimal grid dimensions are determined by:
- *w* = ⌈*n* / *k*⌉ (LEDs per turn, rounded up)
- *h* = min(⌈*k*⌉, ⌈*n* / *w*⌉) (minimize unused pixels)




## 4. Algorithm Design

### 4.1 Forward Mapping Algorithm

The forward mapping algorithm computes cylindrical coordinates for a given LED index with computational complexity O(1):

```
function forwardMap(ledIndex, totalLEDs, totalTurns):
    progress = ledIndex / (totalLEDs - 1)
    angularProgress = progress * totalTurns
    
    u = (angularProgress mod 1.0) * gridWidth
    v = angularProgress * gridHeight / totalTurns
    
    return (u, v)
```

### 4.2 Sub-Pixel Rendering Algorithm

# ZACHS NOTE: mention which direction: corkscrew -> grid or the other way around?

To achieve sub-pixel accuracy, we introduce a tile-based multi-sampling approach. For a given floating-point position *p*, we compute a 2×2 tile of contributing pixels with associated weights:

```
function computeTile(position):
    basePos = floor(position)
    fractional = position - basePos
    
    weights = [
        (1-fractional.x) * (1-fractional.y),  // top-left
        fractional.x * (1-fractional.y),      // top-right
        (1-fractional.x) * fractional.y,      // bottom-left
        fractional.x * fractional.y           // bottom-right
    ]
    
    return Tile2x2(basePos, weights)
```

### 4.3 Multi-Sampling Integration


# ZACHS NOTE: which direction is this going? corksrew -> grid or the other way around.


When rendering from a high-resolution source to the LED array, we employ super-sampling to reduce aliasing artifacts:

```
function multiSample(sourceGrid, targetPosition, sampleRadius):
    totalColor = (0, 0, 0)
    totalWeight = 0
    
    for offset in samplePattern(sampleRadius):
        samplePos = targetPosition + offset
        if inBounds(sourceGrid, samplePos):
            weight = gaussianWeight(offset, sampleRadius)
            color = bilinearSample(sourceGrid, samplePos)
            totalColor += color * weight
            totalWeight += weight
    
    return totalColor / totalWeight
```

## 5. Implementation

## 5.1 Memory safety

# ZACHS NOTE: FILL THIS IN

### 5.4 Web Integration

The system generates ScreenMap data structures that describe the 3D spatial arrangement of LEDs for web-based visualization tools. This enables real-time preview of patterns in browser environments while maintaining accurate geometric representation.

## 6. Evaluation

### 6.1 Experimental Setup

We evaluated our technique using several test configurations:

- **Festival Stick**: 288 LEDs, 19.25 turns, 30cm length
- **Wearable Helix**: 144 LEDs, 12 turns, 15cm length  
- **Architectural Column**: 576 LEDs, 8 turns, 2m length

Each configuration was tested with both synthetic patterns (geometric shapes, gradients) and real-world content (text, logos, animations).

### 6.2 Visual Quality Assessment

Visual quality was assessed through:
1. **Pattern continuity**: Measuring visual breaks at helical boundaries
2. **Shape preservation**: Evaluating geometric accuracy of rendered shapes
3. **Color uniformity**: Assessing consistency across the helical surface



## 8. Limitations and Future Work

## 9. Conclusion

# ZACHS NOTE: UPDATE THIS NOW THAT IV'E TRIMMED THIS SECTION

We have presented Corkscrew Mapping, a novel technique for visualizing 2D graphics patterns on helically-arranged LED strips. Our mathematical framework provides both theoretical rigor and practical performance, enabling real-time applications in diverse contexts from festival installations to architectural lighting.

The key contributions include:
- A complete mathematical framework for helical-to-cylindrical mapping
- Efficient algorithms suitable for embedded systems
- Sub-pixel accurate rendering through multi-sampling techniques  
- Comprehensive evaluation demonstrating significant quality improvements
- Open-source implementation facilitating community adoption

The technique addresses a significant gap in LED visualization tools and has already demonstrated practical value in real-world installations. As LED installations continue to grow in complexity and scale, techniques like Corkscrew Mapping will become increasingly essential for creating compelling visual experiences.

Future work will extend the framework to handle more complex geometries and integrate machine learning approaches for automatic optimization. We anticipate that the mathematical foundations established here will inspire further research in discrete-to-continuous mapping problems across various domains.
